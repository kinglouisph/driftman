compile:
	gcc main.c glad/src/glad.c -odrift -O2 -lcglm -lglfw -lGL -lX11 -lpthread -ldl -lm -Iglad/include
	
windows:
	x86_64-w64-mingw32-gcc main.c glad/src/glad.c -odrift -O2 -lglfw3 -lopengl32 -lgdi32 -Iglad/include