/*
 * Image Viewer/Drawer with .vd File Input & Text Background
 * Loads an image, optionally reads drawing instructions (points with labels, lines) from a .vd file,
 * displays cursor coordinates in title, prints click coordinates to console,
 * draws the loaded lines first, then the loaded points (with white backgrounds for text labels),
 * and allows saving/quitting. Requires SDL2_image and SDL2_ttf.
 *
 * Compile with a Makefile that links SDL2, SDL2_image, SDL2_ttf, and m.
 *
 * .vd file format:
 * point(x,y,"label")
 * line(x1,y1,x2,y2)
 * Lines starting with '#' or empty lines are ignored.
 *
 * Command line usage: ./image_drawer <image_file_path> [drawing_file.vd]
 */

#define _CRT_SECURE_NO_WARNINGS // For some compilers to use functions like sscanf, strcpy safely

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h> // Required for text rendering
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // For malloc, free, strtol
#include <string.h> // For strcpy, strlen, strstr, strcspn
#include <math.h> // Required for thick line and filled circle calculations

// --- Struct Definitions ---
// Struct to hold a point
typedef struct {
    int x;
    int y;
    char* label; // Pointer to the label string (allocated dynamically)
} Point;

// Struct to hold a line
typedef struct {
    Point p1; // Contains x, y. Label field is ignored for lines.
    Point p2; // Contains x, y. Label field is ignored for lines.
} Line;
// --- End of Struct Definitions ---

// Maximum number of drawing elements we can load from the file
#define MAX_DRAW_ELEMENTS 500

// Window dimensions (will be adjusted to image size)
int SCREEN_WIDTH = 800; // Default, will be updated
int SCREEN_HEIGHT = 600; // Default, will be updated

// Colors
const SDL_Color COLOR_BLACK = {0, 0, 0, 255}; // Thick black lines, points, and text
const SDL_Color COLOR_WHITE_BG = {255, 255, 255, 255}; // White background for text

// Drawing parameters
const int DRAW_LINE_THICKNESS = 5; // Thickness of drawn lines
const int DRAW_POINT_RADIUS = 4;   // Radius of the filled circles representing points
const int FONT_SIZE = 12;          // Size of the font for labels

// Function to set drawing color
void set_draw_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

// Function to draw a filled circle
void draw_filled_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
    set_draw_color(renderer, color);

    for (int y = -radius; y <= radius; y++) {
        // Ensure we don't take sqrt of a negative number if radius*radius - y*y is slightly less than 0 due to floating point
        float x_squared = radius * radius - y * y;
        if (x_squared < 0) x_squared = 0; // Clamp to 0

        int x_span = sqrt(x_squared);
        // Draw a horizontal line from cx - x_span to cx + x_span at this y
        SDL_RenderDrawLine(renderer, cx - x_span, cy + y, cx + x_span, cy + y);
    }
}

// Function to draw text using SDL_ttf with a background
void draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font || !text || text[0] == '\0') {
        // Nothing to render if font is null, text is null, or text is empty
        return;
    }

    // Render text surface (Solid rendering creates a solid color text with transparent background)
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text, color);
    if (textSurface == NULL) {
        fprintf(stderr, "Unable to render text surface! TTF_Error: %s\n", TTF_GetError());
        return;
    }

    // Create texture from surface
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (textTexture == NULL) {
        fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
    } else {
        // Get texture dimensions
        int textW, textH;
        SDL_QueryTexture(textTexture, NULL, NULL, &textW, &textH);

        // --- Draw White Background Rectangle ---
        SDL_Rect backgroundRect = { x, y, textW, textH }; // Background position and size
        set_draw_color(renderer, COLOR_WHITE_BG); // Use the white background color
        SDL_RenderFillRect(renderer, &backgroundRect); // <-- This line draws the white rectangle
        // --- End Draw White Background ---

        // Set rendering space for the text texture
        SDL_Rect renderQuad = { x, y, textW, textH };
        // No need to set draw color here again, SDL_RenderCopy uses the texture's colors/alpha

        // Render the text texture on top of the background
        SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);

        // Destroy texture
        SDL_DestroyTexture(textTexture);
    }

    // Free surface
    SDL_FreeSurface(textSurface);
}


// Function to draw a point (draws a filled circle and a label)
void draw_point_with_label(SDL_Renderer* renderer, Point point, int radius, SDL_Color color, TTF_Font* font) {
    // Draw the filled circle for the point
    draw_filled_circle(renderer, point.x, point.y, radius, color);

    // Draw the label beside the point (offset slightly)
    if (point.label) {
        int label_x_offset = radius + 5; // 5 pixels to the right of the circle edge
        int label_y_offset = -radius; // Align top of text roughly with top of circle
        draw_text(renderer, font, point.label, point.x + label_x_offset, point.y + label_y_offset, color);
    }
}


// Function to draw a thick line (approximated by drawing multiple parallel lines)
void draw_thick_line(SDL_Renderer* renderer, Line line, int thickness, SDL_Color color) {
    set_draw_color(renderer, color);

    int x1 = line.p1.x;
    int y1 = line.p1.y;
    int x2 = line.p2.x;
    int y2 = line.p2.y;

    // Calculate the direction vector and its perpendicular
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);

    if (length == 0) {
        // If the line is zero length, draw a thick point (a filled square approximation)
         for (int i = -thickness/2; i <= thickness/2; ++i) {
            for (int j = -thickness/2; j <= thickness/2; ++j) {
                SDL_RenderDrawPoint(renderer, x1 + i, y1 + j);
            }
        }
        return;
    }

    // Normalized perpendicular vector
    float nx = -dy / length;
    float ny = dx / length;

    // Draw multiple lines offset from the original
    for (int i = -thickness / 2; i <= thickness / 2; ++i) {
        SDL_RenderDrawLine(renderer, x1 + (int)(i * nx), y1 + (int)(i * ny), x2 + (int)(i * nx), y2 + (int)(i * ny));
    }
}


// Function to save the renderer's content to a PNG file
// This function is defined ONCE
bool save_screenshot(SDL_Renderer* renderer, int width, int height, const char* filename) {
    printf("Attempting to save screenshot to %s...\n", filename); // Debug print

    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) {
        fprintf(stderr, "Failed to create surface for screenshot: %s\n", SDL_GetError()); // Debug print
        return false;
    }
    printf("Surface created.\n"); // Debug print

    if (SDL_RenderReadPixels(renderer, NULL, surface->format->format, surface->pixels, surface->pitch) != 0) {
        fprintf(stderr, "Failed to read pixels from renderer for screenshot: %s\n", SDL_GetError()); // Debug print
        SDL_FreeSurface(surface);
        return false;
    }
     printf("Pixels read from renderer.\n"); // Debug print


    // Save the surface as PNG
    if (IMG_SavePNG(surface, filename) != 0) {
        fprintf(stderr, "Failed to save surface as PNG: %s\n", IMG_GetError()); // Debug print
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_FreeSurface(surface);
    printf("Screenshot saved successfully to %s.\n", filename); // Debug print
    return true;
}

// Function to parse the drawing file (.vd)
bool parse_drawing_file(const char* filepath, Point* points, int* point_count, Line* lines, int* line_count, int max_elements) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open drawing file %s. Proceeding without drawing data.\n", filepath);
        return false; // File doesn't exist or error, not a fatal error for the program
    }

    char line_buffer[512]; // Buffer to read each line
    *point_count = 0;
    *line_count = 0;

    printf("Parsing drawing file: %s\n", filepath); // Debug print

    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        // Remove trailing newline if present
        line_buffer[strcspn(line_buffer, "\n")] = 0;

        // Skip comments and empty lines
        if (line_buffer[0] == '#' || line_buffer[0] == '\0') {
            continue;
        }

        int x, y, x1, y1, x2, y2;

        // --- Try parsing as a point: point(x,y,"label") ---
        char* point_call_start = strstr(line_buffer, "point(");
        if (point_call_start) {
            char* param_start = point_call_start + strlen("point(");
            char* param_end = strstr(param_start, ")");
             if (param_end) {
                *param_end = '\0'; // Temporarily null-terminate parameters string

                // Parse parameters: x,y,"label"
                char* current_pos = param_start;
                char* first_comma = strchr(current_pos, ',');

                if (first_comma) {
                    *first_comma = '\0'; // Null-terminate x part
                    int read_x = sscanf(current_pos, "%d", &x);
                    current_pos = first_comma + 1; // Move past the comma

                    char* second_comma = strchr(current_pos, ',');
                    if (second_comma) {
                        *second_comma = '\0'; // Null-terminate y part
                        int read_y = sscanf(current_pos, "%d", &y);
                        current_pos = second_comma + 1; // Move past the comma

                        // Look for the label string in quotes
                        char* quote_start = strchr(current_pos, '"');
                        if (quote_start && *(quote_start + 1) != '\0') {
                            char* label_content_start = quote_start + 1;
                            char* quote_end = strchr(label_content_start, '"');
                            if (quote_end) {
                                *quote_end = '\0'; // Null-terminate the label content

                                if (read_x == 1 && read_y == 1) {
                                    if (*point_count < max_elements) {
                                        points[*point_count].x = x;
                                        points[*point_count].y = y;
                                        // Allocate memory for label and copy
                                        points[*point_count].label = malloc(strlen(label_content_start) + 1);
                                        if (points[*point_count].label) {
                                            strcpy(points[*point_count].label, label_content_start);
                                            (*point_count)++;
                                            printf("Parsed Point: (%d, %d, \"%s\")\n", x, y, label_content_start); // Debug print
                                        } else {
                                            fprintf(stderr, "Error allocating memory for label: %s\n", label_content_start);
                                        }
                                    } else {
                                        fprintf(stderr, "Warning: Max points (%d) reached. Skipping point: %s\n", max_elements, line_buffer);
                                    }
                                    // Restore the ')'
                                    *param_end = ')';
                                    continue; // Move to next line after successful parse
                                }
                            }
                        }
                    }
                }
                 // Restore the ')' if it was null-terminated
                *param_end = ')';
            }
             // If parsing failed but it looked like a point call
            fprintf(stderr, "Warning: Failed to parse point format: %s\n", line_buffer);
            continue; // Move to next line

        } // End of point parsing attempt


        // --- Try parsing as a line: line(x1,y1,x2,y2) ---
        char* line_call_start = strstr(line_buffer, "line(");
        if (line_call_start) {
            char* param_start = line_call_start + strlen("line(");
            char* param_end = strstr(param_start, ")");
            if (param_end) {
                *param_end = '\0'; // Temporarily null-terminate

                // Parse the four integers separated by commas
                int read_count = sscanf(param_start, "%d,%d,%d,%d", &x1, &y1, &x2, &y2);

                // Restore the ')'
                *param_end = ')';

                if (read_count == 4) {
                     if (*line_count < max_elements) {
                        lines[*line_count].p1.x = x1;
                        lines[*line_count].p1.y = y1;
                        lines[*line_count].p2.x = x2;
                        lines[*line_count].p2.y = y2;
                        // Labels are not used for line points, set to NULL
                        lines[*line_count].p1.label = NULL;
                        lines[*line_count].p2.label = NULL;

                        (*line_count)++;
                        printf("Parsed Line: (%d, %d) to (%d, %d)\n", x1, y1, x2, y2); // Debug print
                    } else {
                         fprintf(stderr, "Warning: Max lines (%d) reached. Skipping line: %s\n", max_elements, line_buffer);
                    }
                    continue; // Move to next line
                }
            }
             // If parsing failed but it looked like a line call
             fprintf(stderr, "Warning: Failed to parse line format: %s\n", line_buffer);
             continue; // Move to next line
        } // End of line parsing attempt


        // If line was not parsed as point or line
        fprintf(stderr, "Warning: Unrecognized line format: %s\n", line_buffer);

    }

    fclose(file);
    printf("Finished parsing. Loaded %d points and %d lines.\n", *point_count, *line_count); // Debug print
    return true; // File was opened and parsed (warnings might exist)
}


// Function to free memory allocated for point labels
void free_loaded_drawing_data(Point* points, int point_count) {
    for (int i = 0; i < point_count; ++i) {
        free(points[i].label); // free(NULL) is safe
        points[i].label = NULL; // Prevent double freeing
    }
}


int main(int argc, char* argv[]) {
    // Check for required image file argument
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image_file_path> [drawing_file.vd]\n", argv[0]);
        return 1;
    }
    const char* image_path = argv[1];
    const char* drawing_file_path = NULL;
    if (argc > 2) {
        drawing_file_path = argv[2]; // Optional second argument is the drawing file
    }


    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize SDL_image for loading and saving
    int img_flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP; // Add formats you expect
    if (!(IMG_Init(img_flags) & img_flags)) {
        fprintf(stderr, "SDL_image could not initialize! IMG_Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // Initialize SDL_ttf for text rendering
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        IMG_Quit(); // Clean up IMG as well
        SDL_Quit();
        return 1;
    }


    // Load the image
    SDL_Surface* loaded_surface = IMG_Load(image_path);
    if (loaded_surface == NULL) {
        fprintf(stderr, "Failed to load image %s! IMG_Error: %s\n", image_path, IMG_GetError());
        TTF_Quit(); // Clean up TTF as well
        IMG_Quit(); // Clean up IMG
        SDL_Quit();
        return 1;
    }

    // Update window dimensions to match image
    SCREEN_WIDTH = loaded_surface->w;
    SCREEN_HEIGHT = loaded_surface->h;

    // Create window
    SDL_Window* window = SDL_CreateWindow("Image Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_FreeSurface(loaded_surface);
        TTF_Quit(); // Clean up TTF
        IMG_Quit(); // Clean up IMG
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_FreeSurface(loaded_surface);
        TTF_Quit(); // Clean up TTF
        IMG_Quit(); // Clean up IMG
        SDL_Quit();
        return 1;
    }

    // Create texture from surface
    SDL_Texture* image_texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
    if (image_texture == NULL) {
        fprintf(stderr, "Failed to create texture from surface! SDL Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_FreeSurface(loaded_surface);
        TTF_Quit(); // Clean up TTF
        IMG_Quit(); // Clean up IMG
        SDL_Quit();
        return 1;
    }

    // Free the loaded surface (no longer needed after creating texture)
    SDL_FreeSurface(loaded_surface);


    // Load the font for labels
    // You MUST change this to a valid .ttf font file on your system
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; // <--- CHANGE THIS IF NEEDED
    TTF_Font* gFont = TTF_OpenFont(font_path, FONT_SIZE);
    if (gFont == NULL) {
        fprintf(stderr, "Failed to load font %s! TTF_Error: %s\n", font_path, TTF_GetError());
        // Continue without font, point labels won't be drawn
        gFont = NULL; // Ensure gFont is NULL if loading failed
    }


    // --- Load drawing data from .vd file if provided ---
    Point loaded_points[MAX_DRAW_ELEMENTS];
    int loaded_point_count = 0;
    Line loaded_lines[MAX_DRAW_ELEMENTS];
    int loaded_line_count = 0;

    if (drawing_file_path) {
         parse_drawing_file(drawing_file_path, loaded_points, &loaded_point_count, loaded_lines, &loaded_line_count, MAX_DRAW_ELEMENTS);
    }
    // --- End of Loading drawing data ---


    // Event loop
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                printf("SDL_QUIT event received. Quitting...\n"); // Debug print
                quit = true;
            } else if (e.type == SDL_MOUSEMOTION) {
                // Display current cursor coordinates (in window title)
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                char title[100];
                snprintf(title, 100, "Image Viewer - Cursor: (%d, %d)", mouseX, mouseY);
                SDL_SetWindowTitle(window, title);
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    // Print clicked coordinates to console
                    printf("Clicked at: (%d, %d)\n", e.button.x, e.button.y);
                }
            } else if (e.type == SDL_KEYDOWN) {
                printf("Key pressed: %d\n", e.key.keysym.sym); // Debug print key code
                switch (e.key.keysym.sym) {
                    case SDLK_q:
                        printf("'q' key pressed. Quitting...\n"); // Debug print
                        quit = true; // Quit on 'q' key press
                        break;
                    case SDLK_s:
                        printf("'s' key pressed.\n"); // Debug print
                        // Save screenshot on 's' key press
                        // Call the SINGLE save_screenshot function
                        if (save_screenshot(renderer, SCREEN_WIDTH, SCREEN_HEIGHT, "image_with_drawing.png")) {
                            printf("Screenshot initiation successful.\n"); // Debug print
                        } else {
                             fprintf(stderr, "Screenshot initiation failed.\n"); // Debug print
                        }
                        break;
                    // Add other key handlers if needed
                }
            }
        }

        // --- Rendering ---
        // Clear renderer
        set_draw_color(renderer, COLOR_WHITE_BG); // Use white background or clear color
        SDL_RenderClear(renderer);

        // Render the image texture
        SDL_RenderCopy(renderer, image_texture, NULL, NULL);

        // Draw the loaded lines (Draw these first so points are on top)
        for (int i = 0; i < loaded_line_count; ++i) {
             draw_thick_line(renderer, loaded_lines[i], DRAW_LINE_THICKNESS, COLOR_BLACK);
        }

        // Draw the loaded points (as filled circles) with labels (Draw these second)
        for (int i = 0; i < loaded_point_count; ++i) {
            draw_point_with_label(renderer, loaded_points[i], DRAW_POINT_RADIUS, COLOR_BLACK, gFont);
        }

        // Update screen
        SDL_RenderPresent(renderer);
    }

    // Clean up
    // Free memory allocated for point labels
    free_loaded_drawing_data(loaded_points, loaded_point_count);

    // Close font
    if (gFont) { // Only close if font was loaded successfully
        TTF_CloseFont(gFont);
    }
    SDL_DestroyTexture(image_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit(); // Quit TTF subsystem
    IMG_Quit(); // Quit IMG subsystem
    SDL_Quit(); // Quit SDL subsystem

    return 0;
}
