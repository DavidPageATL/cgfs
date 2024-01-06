#define SDL_MAIN_HANDLED

#include <stdlib.h>
#include <SDL.h>

typedef struct {
	int r;
	int g;
	int b;
} Color;

typedef struct {
	float x;
	float y;
	float z;
} Vec3d;

typedef struct {
	Vec3d center;
	float radius;
	Color color;
} Sphere;

static int WIDTH = 600;
static int HEIGHT = 600;

static float VIEWPORT_SIZE = 1;
static float PROJECTION_PLANE_Z = 1;

static float DotProduct(Vec3d v1, Vec3d v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static Vec3d Subtract(Vec3d v1, Vec3d v2) {
	return Vec3d{ v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

static Vec3d CanvasToViewport(int x, int y) {
	return Vec3d{ x * VIEWPORT_SIZE / WIDTH, y * VIEWPORT_SIZE / HEIGHT,  PROJECTION_PLANE_Z };
}

static Vec3d IntersectRaySphere(Vec3d origin, Vec3d direction, Sphere sphere) {
	Vec3d oc = Subtract(origin, sphere.center);

	float k1 = DotProduct(direction, direction);
	float k2 = 2 * DotProduct(oc, direction);
	float k3 = DotProduct(oc, oc) - sphere.radius * sphere.radius;

	float discriminant = k2 * k2 - 4 * k1 * k3;

	if (discriminant < 0) {
		return Vec3d{ INFINITY, INFINITY, INFINITY };
	}

	float t1 = (float)(-k2 + sqrt(discriminant)) / (2 * k1);
	float t2 = (float)(-k2 - sqrt(discriminant)) / (2 * k1);

	return Vec3d{ t1, t2, 0 };
}

static Sphere spheres[] = {
	{{0, -1, 3}, 1, {255, 0, 0}},
	{{2, 0, 4}, 1, {0, 0, 255}},
	{{-2, 0,4}, 1, {0, 255, 0}},
};

static Color TraceRay(Vec3d origin, Vec3d direction, float min_t, float max_t) {
	float closest_t = INFINITY;
	Sphere* closest_sphere = NULL;

	for (int i = 0; i < sizeof(spheres) / sizeof(spheres[0]); i++) {

		Vec3d ts = IntersectRaySphere(origin, direction, spheres[i]);

		if (ts.x < closest_t && min_t < ts.x && ts.x < max_t) {
			closest_t = ts.x;
			closest_sphere = &spheres[i];
		}

		if (ts.y < closest_t && min_t < ts.y && ts.y < min_t) {
			closest_t = ts.y;
			closest_sphere = &spheres[i];
		}
	}

	if (closest_sphere == NULL) {
		return Color{ 255, 255, 255 };
	}

	return closest_sphere->color;
}

static void PutPixel(SDL_Renderer* renderer, int x, int y, int r, int g, int b) {
	x = WIDTH / 2 + x;
	y = HEIGHT / 2 - y - 1;

	if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
		return;
	}

	SDL_SetRenderDrawColor(renderer, r, g, b, 255);
	SDL_RenderDrawPoint(renderer, x, y);
}

extern C_LINKAGE int SDL_main(int argc, char* argv[]) {

	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("SDL failed to initialise: %s\n", SDL_GetError());
		return 1;
	}

	if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) != 0) {
		SDL_Log("Failed to create window and renderer %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
	SDL_RenderClear(renderer);

	Vec3d camera_position{ 0, 0, 0 };

	for (int x = -WIDTH / 2; x < WIDTH / 2; x++) {
		for (int y = -HEIGHT / 2; y < HEIGHT / 2; y++) {
			Vec3d direction = CanvasToViewport(x, y);
			Color color = TraceRay(camera_position, direction, 1, INFINITY);
			PutPixel(renderer, x, y, color.r, color.g, color.b);
		}
	}

	SDL_RenderPresent(renderer);

	while (true) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				break;
			}
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return EXIT_SUCCESS;
}