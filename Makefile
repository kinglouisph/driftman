compile:
	gcc main.c glad/src/glad.c -otest -O2 -lcglm -lglfw -lGL -lX11 -lpthread -ldl -lm -Iglad/include