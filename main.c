#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <cglm/cglm.h>

uint32_t VBO;
uint32_t VAO;
uint32_t EBO;
unsigned int shaderProgram;

float camx = 0.0f;
float camy = 0.0f;
const uint32_t windowWidth=800;
const uint32_t windowHeight=600;

//mouse data
float mx=0.001f;
float my=0.001f;
bool mdown = 0;
bool plAlive = 1;

vec2 plPos = {0.0f,0.0f};
vec2 plVel = {0.0f,0.0f};
float plAccSpeed = 0.05f;
float plTurnspeed = 0.05f;
float plAngle = 0.0f;
const float plSize = 12.5f;
const float explosionSize = 12.5f;

float plVerts[] = {
    1.0f,0.0f,
    cos(M_PI*5/6), -sin(M_PI*5/6),
    cos(M_PI*5/6), sin(M_PI*5/6)
};

int plTrailLength = 10;
int plTrailNum=0;
int plTrailSize = 10*6;
float plTrailVals[10*6];



const char* getGLErrorStr(GLenum err)
{
    switch (err)
    {
    case GL_NO_ERROR:          return "No error";
    case GL_INVALID_ENUM:      return "Invalid enum";
    case GL_INVALID_VALUE:     return "Invalid value";
    case GL_INVALID_OPERATION: return "Invalid operation";
    //case GL_STACK_OVERFLOW:    return "Stack overflow";
    //case GL_STACK_UNDERFLOW:   return "Stack underflow";
    case GL_OUT_OF_MEMORY:     return "Out of memory";
    default:                   return "Unknown error";
    }
}

void mouse_callback(GLFWwindow* window, double dmposx, double dmposy) {
    mx = (float)(dmposx - windowWidth/2);
    my = -1.0f*(float)(dmposy - windowHeight/2);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        mdown = 1;
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        mdown = 0;
    }
}

const unsigned int bezierApproxSize = 80; //min 60

typedef struct Bezier {
    float xa; //proportional to t^3
    float xb;
    float xc;
    float xd;
    float ya;
    float yb;
    float yc;
    float yd;
    
    float dxa; //proportional to t^2
    float dxb;
    float dxc;
    float dya;
    float dyb;
    float dyc;
    vec2 approxPoints[80]; //bezierApproxSize
} Bezier;

Bezier* createBezier(float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4) {
    Bezier* bez = malloc(sizeof(Bezier));
    bez->xa = (x1*-1)+(3*x2)+(-3*x3)+(x4);
    bez->xb = (x1*3)+(x2*-6)+(x3*3);
    bez->xc = (x1*-3)+(x2*3);
    bez->xd = (x1*1);
    bez->ya = (y1*-1)+(3*y2)+(-3*y3)+(y4);
    bez->yb = (y1*3)+(y2*-6)+(y3*3);
    bez->yc = (y1*-3)+(y2*3);
    bez->yd = (y1*1);
    
    bez->dxa = (x1*-3)+(x2*9)+(x3*-9)+(x4*3);
    bez->dxb = (x1*6)+(x2*-12)+(x3*6);
    bez->dxc = (x1*-3)+(x2*3);
    bez->dya = (y1*-3)+(y2*9)+(y3*-9)+(y4*3);
    bez->dyb = (y1*6)+(y2*-12)+(y3*6);
    bez->dyc = (y1*-3)+(y2*3);
    
    float t = 0.0f;
    float ts = 1.0f / (float)(bezierApproxSize-1);
    for (int i = 0; i < bezierApproxSize; i++) {
        float x = bez->xa*t*t*t + bez->xb*t*t + bez->xc*t + bez->xd;
        float y = bez->ya*t*t*t + bez->yb*t*t + bez->yc*t + bez->yd;
        
        bez->approxPoints[i][0] = x;
        bez->approxPoints[i][1] = y;
        t+=ts;
    }
    
    return bez;
}

int numBeziers = 3;
Bezier* beziers[3];

float explosion1Verts[] = {
    1.0f,0.0f,
    0.0f,1.0f,
    -1.0f,0.0f,
    0.0f,-1.0f,
    0.5f,0.5f,
    -0.5f,0.5f,
    -0.5f,-0.5f,
    0.5f,-0.5f,
};

unsigned char explosion1Elems[] = {
    7,0,4,
    4,1,5,
    5,2,6,
    6,3,7
};

float explosion1Colors[4] = {0.98f,0.04f,0.02f,1.0f};

float explosion2Verts[] = {
    0.85f,0.85f,
    -0.85f,0.85f,
    -0.85f,-0.85f,
    0.85f,-0.85f,
    0.52f,0.0f,
    0.0f,0.52f,
    -0.52f,0.0f,
    0.0f,-0.52f,
};

unsigned char explosion2Elems[] = {
    4,0,5,
    5,1,6,
    6,2,7,
    7,3,4,
    6,5,4,
    6,7,4
};

float explosion2Colors[4] = {0.98f,0.65f,0.02f,1.0f};


int main() {
    const char* vertexShaderSrc = "#version 330 core\n"
    "uniform mat4 projection;"
    "uniform vec2 camPos;"
    "layout (location=0) in vec2 triPos;"
    "void main(){"
    "vec2 npos=triPos - camPos;"
    "gl_Position=projection * vec4(npos,0,1);}\0";

    const char* fragmentShaderSrc = "#version 330 core\n"
    "out vec4 outColor;"
    "uniform vec4 color;"
    "void main(){outColor=vec4(color);}\0";

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(windowWidth,windowHeight,"test",NULL,NULL);
    if(window==NULL){
        printf("no window :(");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        printf("Failed to initialize GLAD\n");
        return -1;
    }   
    
    //capture mouse
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
    glfwSetCursorPosCallback(window, mouse_callback);  
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    glViewport(0,0,windowWidth,windowHeight);
    
    
    //shaders
    int success;
    char infoLog[512];
    uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader,1,&vertexShaderSrc,NULL);
    glCompileShader(vertexShader);

    glGetShaderiv(vertexShader,GL_COMPILE_STATUS,&success);
    if(!success){
        glGetShaderInfoLog(vertexShader,512,NULL,infoLog);
        printf(infoLog);
        return 1;
    }

    uint32_t fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader,1,&fragmentShaderSrc,NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader,GL_COMPILE_STATUS,&success);
    if(!success){
        glGetShaderInfoLog(fragmentShader,512,NULL,infoLog);
        printf(infoLog);
        return 1;
    }

    shaderProgram=glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram,GL_LINK_STATUS,&success);
    
    
    if(!success){
        glGetProgramInfoLog(shaderProgram,512,NULL,infoLog);
        printf(infoLog);
        return 1;
    }


    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    


    float pointBuffer[sizeof(float)*2*bezierApproxSize];
    unsigned char elementBuffer[bezierApproxSize*2];

    glUseProgram(shaderProgram);
    
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pointBuffer), &pointBuffer, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elementBuffer), &elementBuffer, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    
    //glBindVertexArray(0);








    //program stuff
    //onst int cosaLocation = glGetUniformLocation(shaderProgram, "cosa");
    //onst int sinaLocation = glGetUniformLocation(shaderProgram, "sina");
    //const int spritePosLocation = glGetUniformLocation(shaderProgram, "spritePos");
    const int camPosLocation = glGetUniformLocation(shaderProgram, "camPos");
    const int colorLocation = glGetUniformLocation(shaderProgram, "color");
    const int projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    
    float aspect = (float)windowWidth/(float)windowHeight;
    mat4 projection;
    glm_ortho(-1.0f*windowWidth/2,windowWidth/2,-1.0f*windowHeight/2,windowHeight/2,-1,1, projection);
    
    glUniformMatrix4fv(projectionLocation, 1, 0, (float*)projection);
    
    Bezier* bez = createBezier(0,-300,200,600,600,-400,-200,300);
    
    /*for (int i=0;i<60;i++){
        printf("%f %f\n",bez->approxPoints[i][0],bez->approxPoints[i][1]);
    }*/
    
    for (int i=0;i<6;i++){
        plVerts[i]*=plSize;
    }
    
    for (int i=0;i<16;i++){
        explosion1Verts[i]*=explosionSize;
        explosion2Verts[i]*=explosionSize;
    }
    
    for (int i=0;i<plTrailLength*3;i++){
        plTrailVals[i]=-99999999999.0f;
    }
    
    float adjPlVerts[6];
    float adjExplosion1Verts[16];
    float adjExplosion2Verts[16];
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            plAlive = 0;
        }
        
        if (plAlive) {
            float mouseDist = sqrt(mx*mx+my*my);
            float t1=mx / mouseDist;
            float t2=my / mouseDist;
                 
            plAngle = atan2(t2,t1);
            
            if (mdown) {
                plVel[0] += t1 * plAccSpeed;
                plVel[1] += t2 * plAccSpeed;
            }
        }
        
        plPos[0] += plVel[0];
        plPos[1] += plVel[1];
        
        //render
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUniform2f(camPosLocation, plPos[0],plPos[1]);
        
        //player trail
        glUniform4f(colorLocation, 0.95f,0.95f,0.95f,0.1f);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(plTrailVals),plTrailVals);
        glDrawArrays(GL_TRIANGLES,0,plTrailSize);
        
        //draw bezier
        glUniform4f(colorLocation, 0.4f,0.8f,1.0f,0.6f);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*2*bezierApproxSize,&(bez->approxPoints));
        
        glDrawArrays(GL_LINE_STRIP,0,bezierApproxSize);
        
        
        
        
            
        //draw player
        if (plAlive) {
            
            float sina = sin(plAngle);
            float cosa = cos(plAngle);
            
            for (int i = 0; i < 6; i+=2) {
                adjPlVerts[i] = plVerts[i]*cosa - plVerts[i+1]*sina + plPos[0];
                adjPlVerts[i+1] = plVerts[i]*sina + plVerts[i+1]*cosa + plPos[1];
                plTrailVals[(i+plTrailNum)]=adjPlVerts[i];
                plTrailVals[(i+1+plTrailNum)]=adjPlVerts[i+1];
            }
            plTrailNum=(plTrailNum+6)%plTrailSize;
            glUniform4f(colorLocation, 0.95f,0.95f,0.95f,1.0f);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(adjPlVerts),&adjPlVerts);
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        
        if (!plAlive) {
            for (int i = 0; i < 16; i+=2) {
                adjExplosion1Verts[i] = explosion1Verts[i] + plPos[0];
                adjExplosion1Verts[i+1] = explosion1Verts[i+1] + plPos[1];
                adjExplosion2Verts[i] = explosion2Verts[i] + plPos[0];
                adjExplosion2Verts[i+1] = explosion2Verts[i+1] + plPos[1];
            }
            glUniform4fv(colorLocation,1,explosion1Colors);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof((explosion1Verts)),&adjExplosion1Verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,sizeof(explosion1Elems),&explosion1Elems);
            glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, 0);
            
            
            glUniform4fv(colorLocation,1,explosion2Colors);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(explosion2Verts),&adjExplosion2Verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,sizeof(explosion2Elems),explosion2Elems);
            glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_BYTE, 0);
        }
        glfwSwapBuffers(window);
        
        //error test
        GLenum e = glGetError();
        if (e) {
            printf(getGLErrorStr(e));
            fflush(stdout);
        }
        
        usleep(1000000/100); //100fps
    }
    
}