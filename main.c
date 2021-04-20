#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLT_IMPLEMENTATION
#include <glText/gltext.h>

#include <c-utils/gl-utils.h>
#include <c-utils/io.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define BOARD_SIZE 40
#define WINDOW_SIZE 400
#define MAINLOOP_TIMESTEP 100000

enum Snake_Direction
{
	SD_UP = 0,
	SD_LEFT = 1,
	SD_DOWN = 2,
	SD_RIGHT = 3,
};

enum Key_Pressed
{
	KP_LEFT = 0,
	KP_RIGHT = 1,
};

enum Snake_Collision_Type
{
	SCT_empty = 0,
	SCT_food = 1,
	SCT_snake = 2,
};

enum Cell_Type
{
	CT_food = 1 << 30,
	CT_snake = 1 << 31,
	CT_empty = 0,
};

typedef struct GlContext
{
	GLFWwindow* window;
	GLuint shader_program;
	GLuint pos_uniform, board_size_uniform, in_color_uniform;
} GlContext;

typedef struct Snake
{
	int x, y;
	int direction;
	int len;
} Snake;

typedef struct Food
{
	int x, y;
} Food;

typedef struct GameState
{
	unsigned int* grid_buffer;
	Snake snake;
	Food food;
	int score;
	bool key_pressed[2];
	bool done;
} GameState;

float vertices[][2] = {
	{0, 0},
	{0, 1},
	{1, 0},
	{1, 1},
};

float blue[3] = {0.203921568627451, 0.2862745098039216, 0.3647058823529412};
float green[3] = {0.1490196078431373, 0.8, 0.4588235294117647};
float orange[3] = {0.9058823529411765, 0.4901960784313725, 0.1568627450980392};

static inline int calc_grid_index(int x, int y)
{
	return x * BOARD_SIZE + y;
}

void create_and_bind_vertex_buffers()
{
	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
}

void draw_square(GlContext gl_context, float* color_triple, int x, int y)
{
	glUniform2f(gl_context.pos_uniform, x, y);
	glUniform3f(gl_context.in_color_uniform, color_triple[0], color_triple[1], color_triple[2]);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void draw_grid(GlContext gl_context, GameState game_state)
{
	for (int x_pos = 0; x_pos < BOARD_SIZE; x_pos++)
		for (int y_pos = 0; y_pos < BOARD_SIZE; y_pos++)
		{
			int elem = game_state.grid_buffer[calc_grid_index(x_pos, y_pos)];

			if (elem & CT_snake)
			{
				draw_square(gl_context, green, x_pos, y_pos);
			}

			if (elem == CT_food)
			{
				draw_square(gl_context, orange, x_pos, y_pos);
			}
		}
}

void handle_input(GlContext gl_context, GameState* game_state)
{
	if (glfwGetKey(gl_context.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(gl_context.window, GLFW_TRUE);

	if (glfwGetKey(gl_context.window, GLFW_KEY_LEFT) == GLFW_PRESS &&
		!game_state->key_pressed[KP_LEFT])
	{
		game_state->key_pressed[KP_LEFT] = true;
		game_state->snake.direction = game_state->snake.direction + 1;
	}

	if (glfwGetKey(gl_context.window, GLFW_KEY_LEFT) == GLFW_RELEASE)
		game_state->key_pressed[KP_LEFT] = false;

	if (glfwGetKey(gl_context.window, GLFW_KEY_RIGHT) == GLFW_PRESS &&
		!game_state->key_pressed[KP_RIGHT])
	{
		game_state->key_pressed[KP_RIGHT] = true;
		game_state->snake.direction = game_state->snake.direction - 1;
	}

	if (glfwGetKey(gl_context.window, GLFW_KEY_RIGHT) == GLFW_RELEASE)
		game_state->key_pressed[KP_RIGHT] = false;

	game_state->snake.direction = (game_state->snake.direction + 4) % 4;
}

void move_snake(GlContext gl_context, GameState* game_state)
{
	switch (game_state->snake.direction)
	{
		case SD_UP:
			game_state->snake.y += 1;
			break;
		case SD_LEFT:
			game_state->snake.x -= 1;
			break;
		case SD_DOWN:
			game_state->snake.y -= 1;
			break;
		case SD_RIGHT:
			game_state->snake.x += 1;
			break;
	}

	if (game_state->snake.x >= BOARD_SIZE || game_state->snake.y >= BOARD_SIZE ||
		game_state->snake.x < 0 || game_state->snake.y < 0)
		game_state->done = true;
}

int update_snake_on_board(GlContext gl_context, GameState* game_state)
{
	int grid_index = calc_grid_index(game_state->snake.x, game_state->snake.y);
	if (game_state->grid_buffer[grid_index] & CT_food)
	{
		game_state->grid_buffer[grid_index] = CT_snake | game_state->snake.len;
		return SCT_food;
	}
	else if (game_state->grid_buffer[grid_index] & CT_snake)
	{
		return SCT_snake;
	}
	else if (game_state->grid_buffer[grid_index] == 0)
	{
		game_state->grid_buffer[grid_index] = CT_snake | game_state->snake.len;
		return SCT_empty;
	}
	else
		assert(false); // Unreachable
}

void update_board(GlContext gl_context, GameState* game_state)
{
	for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
	{
		if (game_state->grid_buffer[i] == CT_snake)
			game_state->grid_buffer[i] = 0;

		if (game_state->grid_buffer[i] & CT_snake)
			game_state->grid_buffer[i] -= 1;
	}
}

void handle_collision(GlContext gl_context, GameState* game_state, int collision_type)
{
	if (collision_type == SCT_food)
	{
		Food new_food = {rand() % BOARD_SIZE, rand() % BOARD_SIZE};

		while (game_state->grid_buffer[calc_grid_index(new_food.x, new_food.y)] != CT_empty)
			new_food = (Food){rand() % BOARD_SIZE, rand() % BOARD_SIZE};

		game_state->snake.len += 1;
		game_state->score += 1;
		game_state->grid_buffer[calc_grid_index(new_food.x, new_food.y)] = CT_food;
	}

	if (collision_type == SCT_snake)
	{
		game_state->done = true;
		printf("Done\n");
	}
}

void mainloop(GlContext gl_context, GameState* game_state)
{
	move_snake(gl_context, game_state);
	int collision_type = update_snake_on_board(gl_context, game_state);
	handle_collision(gl_context, game_state, collision_type);

	update_board(gl_context, game_state);

	glClear(GL_COLOR_BUFFER_BIT);
	draw_grid(gl_context, *game_state);

	glfwSwapBuffers(gl_context.window);
	glfwPollEvents();
}

GlContext setup_opengl_context()
{
	GlContext gl_context = {
		.window = CU_gl_setup_and_create_window(WINDOW_SIZE, WINDOW_SIZE, "Snake"),
		.shader_program = CU_gl_basic_prog_from_filename("shader.vert", "shader.frag"),

		.pos_uniform = glGetUniformLocation(gl_context.shader_program, "pos"),
		.board_size_uniform = glGetUniformLocation(gl_context.shader_program, "board_size"),
		.in_color_uniform = glGetUniformLocation(gl_context.shader_program, "in_color"),
	};

	glUseProgram(gl_context.shader_program);
	glUniform1f(gl_context.board_size_uniform, BOARD_SIZE);

	glfwSetInputMode(gl_context.window, GLFW_STICKY_KEYS, GLFW_TRUE);

	return gl_context;
}

GameState setup_game_state()
{
	GameState game_state = {
		.grid_buffer = calloc(BOARD_SIZE * BOARD_SIZE, sizeof(int)),
		.snake = {.x = 3, .y = 3, .direction = 0, .len = 3},
		.food = {.x = rand() % BOARD_SIZE, .y = rand() % BOARD_SIZE},
		.done = false,
		.score = 0,
	};

	return game_state;
}

void display_score(GlContext gl_context, GameState* game_state)
{
	gltInit();

	char buffer[20];
	sprintf(buffer, "Score: %d", game_state->score);

	GLTtext* text = gltCreateText();
	gltSetText(text, buffer);

	int viewport_width, viewport_height;
	glfwGetFramebufferSize(gl_context.window, &viewport_width, &viewport_height);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	while (!glfwWindowShouldClose(gl_context.window))
	{
		if (glfwGetKey(gl_context.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(gl_context.window, GLFW_TRUE);

		glClear(GL_COLOR_BUFFER_BIT);

		gltBeginDraw();

		gltColor(orange[0], orange[1], orange[2], 1);
		gltDrawText2DAligned(text, (float) viewport_width / 2, (float) viewport_height / 10,
			5.0, GLT_CENTER, GLT_CENTER);

		gltEndDraw();

		glfwSwapBuffers(gl_context.window);
		glfwPollEvents();
	}
}

void seed_rng()
{
	FILE* fp = fopen("/dev/urandom", "r");

	char buffer[4];
	for (int i = 0; i < 4; i++)
		buffer[i] = fgetc(fp);

	srand(*(int*) buffer);

	fclose(fp);
}

int main()
{
	seed_rng();
	GlContext gl_context = setup_opengl_context();
	GameState game_state = setup_game_state();

	create_and_bind_vertex_buffers();

	glClearColor(blue[0], blue[1], blue[2], 1.0f);
	game_state.grid_buffer[calc_grid_index(game_state.food.x, game_state.food.y)] = CT_food;

	struct timeval start, end;
	gettimeofday(&start, NULL);
	int delta = 0;

	while (!glfwWindowShouldClose(gl_context.window))
	{
		gettimeofday(&end, NULL);
		delta += (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
		start = end;

		handle_input(gl_context, &game_state);

		if (delta > MAINLOOP_TIMESTEP)
		{
			mainloop(gl_context, &game_state);

			if (game_state.done)
				break;

			delta -= MAINLOOP_TIMESTEP;
		}
	}

	display_score(gl_context, &game_state);

	glfwDestroyWindow(gl_context.window);
}
