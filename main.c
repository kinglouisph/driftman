#ifdef WIN32
void* __chk_fail=0; //janky fix
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
//#include <cglm/cglm.h>

typedef char bool;
typedef float vec2[2];

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
bool phaseShift = 0;

vec2 plPos = {0.0f,0.0f};
vec2 plVel = {0.0f,0.0f};
float plAccSpeed = 0.1f;
float plTurnspeed = 0.05f;
float plAngle = 0.0f;
const float plSize = 12.5f;
const float explosionSize = 12.5f;
const float frictionCoefficient = 0.015f;
const float coinSize = 18.0f;
const float coinSizeSquared = coinSize*coinSize;
int coinCount = 0;
float coinx,coiny;

float plVerts[] = {
    1.0f,0.0f,
    cos(M_PI*5/6), -sin(M_PI*5/6),
    cos(M_PI*5/6), sin(M_PI*5/6)
};

int plTrailNum=0;
int plTrailLength = 20;
int plTrailSize = 20*6;
float plTrailVals[20*6];

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

float randf() {
    return (float)rand() / (float)RAND_MAX;
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

const unsigned int bezierApproxSize = 80; //if this is too low other stuff will break
const float bezierSideLength = 90.0f;

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
    vec2 approxSidePoints[80*2]; //for triangle strip
} Bezier;

int numBeziers = 3;
Bezier* beziers[3]; //circular array
int bezierNum = 0;
vec2 lastp3;
vec2 lastp4;
float lastAngle;


int killerBez = 0;

Bezier* createBezier(float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4) {
    lastp3[0]=x3;
    lastp3[1]=y3;
    lastp4[0]=x4;
    lastp4[1]=y4;
    
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
    
    float t = 0.00001f;
    float ts = (1.0f-2*t) / (float)(bezierApproxSize-1);
    for (int i = 0; i < bezierApproxSize; i++) {
        float x = bez->xa*t*t*t + bez->xb*t*t + bez->xc*t + bez->xd;
        float y = bez->ya*t*t*t + bez->yb*t*t + bez->yc*t + bez->yd;
        
        bez->approxPoints[i][0] = x;
        bez->approxPoints[i][1] = y;
        t+=ts;
    }
    
    t = 0.00001f;
    //printf("\n");
    for (int i = 0; i < bezierApproxSize; i++) {
        float dx = bez->dxa*t*t + bez->dxb*t + bez->dxc;
        float dy = bez->dya*t*t + bez->dyb*t + bez->dyc;
        
        float dl = sqrt(dx*dx+dy*dy);
        float temp = dx;
        
        dx = -dy/dl*bezierSideLength;
        dy = temp/dl*bezierSideLength;
        
        //printf("%f %f %f\n",dx,dy,t);
        
        
        float x = bez->approxPoints[i][0];
        float y = bez->approxPoints[i][1];
        
        bez->approxSidePoints[(i<<1)][0] = x+dx;
        bez->approxSidePoints[(i<<1)][1] = y+dy;
        bez->approxSidePoints[(i<<1)+1][0] = x-dx;
        bez->approxSidePoints[(i<<1)+1][1] = y-dy;
        t+=ts;
    }
    
    if (bezierNum == killerBez) {
        killerBez = (killerBez+1)%numBeziers;
    }
    
    t = 0.99999f;
    //these arent actually sin or cos of anything
    //oh well
    float lastCos = bez->dxa*t*t + bez->dxb*t + bez->dxc;
    float lastSin = bez->dya*t*t + bez->dyb*t + bez->dyc;
    //float l = sqrt(lastSin*lastSin+lastCos*lastCos);
    //lastCos/=l;
    //lastSin/=l;
    
    lastAngle = atan2(lastSin,lastCos);
    
    beziers[bezierNum] = bez;
    bezierNum = (bezierNum+1)%numBeziers;
    
    int a = rand()%bezierApproxSize;
    float b = randf()*2*M_PI;
    float c = randf()*bezierSideLength;
    coinx = bez->approxPoints[a][0]+cos(b)*c;
    coiny = bez->approxPoints[a][1]+sin(b)*c;
    
    return bez;
}



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


float trifanSize = 41; //index 0 is center
float trifanArray[41*2];

float trifan2Size = 17; //index 0 is center
float trifan2Array[17*2];

float numberSize = 50.0f;
float numberVerts[] = {
    -0.5f,1.0f,
    0.5f,1.0f,
    -0.5f,0.0f,
    0.5f,0.0f,
    -0.5f,-1.0f,
    0.5f,-1.0f,
};

unsigned char numbersElemsLengths[] = {
    5,3,6,7,5,6,6,3,7,5
};
unsigned char numbersElems[][7] = {
    {
        0,1,5,4,0
    },
    {
        1,3,5
    },
    {
        0,1,3,2,4,5
    },
    {
        0,1,3,2,3,5,4
    },
    {
        0,2,3,1,5
    },
    {
        1,0,2,3,5,4
    },
    {
        1,0,4,5,3,2
    },
    {
        0,1,5
    },
    {
        2,0,1,5,4,2,3
    },
    {
        5,1,0,2,3
    }
};



float killert = 0.000005f;
float killerts = 0.005f;
float killerta = 0.00000025f;
float killerx, killery;

int main() {
    srand (time(NULL));
    
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

    


    //float pointBuffer[sizeof(float)*2*bezierApproxSize];
    //unsigned char elementBuffer[bezierApproxSize*2];

    glUseProgram(shaderProgram);
    
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*4*bezierApproxSize*numBeziers, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 60, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    
    //glBindVertexArray(0);








    //program stuff
    const int camPosLocation = glGetUniformLocation(shaderProgram, "camPos");
    const int colorLocation = glGetUniformLocation(shaderProgram, "color");
    const int projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    
    float aspect = (float)windowWidth/(float)windowHeight;
    //mat4 projection;
    //glm_ortho(-1.0f*windowWidth/2,windowWidth/2,-1.0f*windowHeight/2,windowHeight/2,-1,1, projection);
    
    float projection[] = {
        0.002500, 0.000000, 0.000000, 0.000000,
        0.000000, 0.003333, 0.000000, 0.000000,
        0.000000, 0.000000, -1.000000, 0.000000,
        -0.000000, -0.000000, -0.000000, 1.000000
    };
    
    //printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
    //    projection[0][0],projection[0][1],projection[0][2],projection[0][3],
    //    projection[1][0],projection[1][1],projection[1][2],projection[1][3],
    //    projection[2][0],projection[2][1],projection[2][2],projection[2][3],
    //    projection[3][0],projection[3][1],projection[3][2],projection[3][3]
    //);
    
    glUniformMatrix4fv(projectionLocation, 1, 0, projection);
    
    for (int i=0;i<6;i++){
        plVerts[i]*=plSize;
    }
    
    for (int i=0;i<12;i++){
        numberVerts[i]*=numberSize;
    }
    
    for (int i=0;i<16;i++){
        explosion1Verts[i]*=explosionSize;
        explosion2Verts[i]*=explosionSize;
    }
    
    for (int i=0;i<plTrailLength*3;i++){
        plTrailVals[i]=-99999999999.0f;
    }
    
    float adjPlVerts[6];
    
    float a = 0.0f;
    float as = M_PI*2/(float)(trifanSize-2);
    trifanArray[0]=0.0f;
    trifanArray[1]=0.0f;
    for (int i = 2; i<trifanSize*2;i+=2) {
        trifanArray[i] = bezierSideLength * cos(a);
        trifanArray[i+1] = bezierSideLength * sin(a);
        a+=as;
    }
    
    a = 0.0f;
    as = M_PI*2/(float)(trifan2Size-2);
    trifan2Array[0]=0.0f;
    trifan2Array[1]=0.0f;
    for (int i = 2; i<trifan2Size*2;i+=2) {
        trifan2Array[i] = coinSize * cos(a);
        trifan2Array[i+1] = coinSize * sin(a);
        a+=as;
    }
    
    const float bezierSideLengthSquared = bezierSideLength * bezierSideLength;
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
        createBezier(-600,0,-600,0,-300,0,-300,0);
        createBezier(-600,0,-600,0,-300,0,-300,0);
        createBezier(-600,0,-600,0,-300,0,-300,0);
        createBezier(-300,0,-300,0,300,0,300,0);
        createBezier(300,0,300,0,400,0,600,0);
        //lastAngle = 0.0f;
        
        //pain
        
        
        /*
        #else
        if (PID == 0) {
            execve("mpg123 audio/rocks.mp3");
            return 0;
        }
        #endif*/
        
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
            plAlive = 0;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            phaseShift=1;
        } else {
            phaseShift=0;
        }
        
        if (plAlive && !phaseShift) {
            float mouseDist = sqrt(mx*mx+my*my);
            float t1=mx / mouseDist;
            float t2=my / mouseDist;
                 
            plAngle = atan2(t2,t1);
            
            if (mdown) {
                
                plVel[0] += t1 * plAccSpeed;
                plVel[1] += t2 * plAccSpeed;
            }
            
            //float plSpeed = sqrt(plVel[0]*plVel[0]+plVel[1]*plVel[1]);
            //float t3 = plVel[1];
            
            plVel[0] -= plVel[0] * frictionCoefficient * fabs(t2);
            plVel[1] -= plVel[1] * frictionCoefficient * fabs(t1);

            
        }
        
        plPos[0] += plVel[0];
        plPos[1] += plVel[1];
        
        float targetx = plPos[0] - beziers[(bezierNum+numBeziers-1)%numBeziers]->approxPoints[bezierApproxSize-1][0];
        float targety = plPos[1] - beziers[(bezierNum+numBeziers-1)%numBeziers]->approxPoints[bezierApproxSize-1][1];
        
        if (!phaseShift && targetx*targetx+targety*targety < bezierSideLengthSquared) {
            vec2 temp;
            temp[0] = 2*lastp4[0] - lastp3[0];
            temp[1] = 2*lastp4[1] - lastp3[1];
            
            float a1 = (randf()*M_PI-M_PI_2)*5/6 + lastAngle;
            float a2 = (randf()*M_PI-M_PI_2)*5/6 + lastAngle;
            float m1 = randf() * 950.0f + 150.0f;
            float m2 = randf() * 700.0f + 300.0f;
            
            float a = cos(a1) * m1 + lastp4[0];
            float b = sin(a1) * m1 + lastp4[1];
            float c = cos(a2) * m2 + lastp4[0];
            float d = sin(a2) * m2 + lastp4[1];
            
            
            
            //float a = lastp4[0] + (randf()+0.5f) * (float)windowWidth * 0.35f;
            //float b = lastp4[1] + (randf()-0.5f) * (float)windowHeight * 0.5f; 
            //float c = lastp4[0] + (randf()+1.0f) * (float)windowWidth * 0.35f;
            //float d = lastp4[1] + (randf()-0.5f) * (float)windowHeight * 0.5f; 
            //
            //a = a*lastCos - b*lastSin;
            //b = a*lastSin + b*lastCos;
            //c = c*lastCos - d*lastSin;
            //d = c*lastSin + d*lastCos;
            
            
            createBezier(lastp4[0],lastp4[1],temp[0],temp[1],a,b,c,d);
        }
        Bezier* bez = beziers[killerBez];
        killerx = plPos[0] - (killert*killert*killert*bez->xa + killert*killert*bez->xb + killert*bez->xc + bez->xd);
        killery = plPos[1] - (killert*killert*killert*bez->ya + killert*killert*bez->yb + killert*bez->yc + bez->yd);
        
        float aCoinx = plPos[0] - coinx;
        float aCoiny = plPos[1] - coiny;
        
        if (plAlive) {
            
            if (!phaseShift && aCoinx*aCoinx+aCoiny*aCoiny < coinSizeSquared) {
                coinx=-9999999999999999999.0f;
                coiny=-9999999999999999999.0f;
                aCoinx=-9999999999999999999.0f;
                aCoiny=-9999999999999999999.0f;
                coinCount++;
            }
            
            
            //float dkx = killert*killert*bez->dxa+killert*bez->dxb+bez->dxc;
            //float dky = killert*killert*bez->dxa+killert*bez->dxb+bez->dxc;
            //float dkl = sqrt(dkx*dkx+dky*dky);
            
            
            
            
            if (!phaseShift && killerx*killerx+killery*killery < bezierSideLengthSquared) {
                plAlive = 0;
            }
            
            float a = killerx - targetx;
            float b = killery - targety;
            float c = a*a+b*b;
            
            if (c > 4*bezierSideLengthSquared) {
                killert += killerts;///(1.0f+dkl);
                killerts += killerta;
            }
            
            
            if (killert > 0.9999f) {
                killert = 0.00001f;
                killerBez=(killerBez+1)%numBeziers;
            }
        }
        
        if (!phaseShift && plAlive) {
            plAlive = 0;
            for (int i=0;i<numBeziers;i++) {
                for (int j=0;j<bezierApproxSize;j++) {
                    float a = beziers[i]->approxPoints[j][0] - plPos[0];
                    float b = beziers[i]->approxPoints[j][1] - plPos[1];
                    if (a*a+b*b < bezierSideLengthSquared) {
                        plAlive = 1;
                        break;
                    }
                    
                }
                if (plAlive == 1) {
                    break;
                }
            }
        }
        
        
        //render
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        
        glClearColor(0.11f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUniform2f(camPosLocation, plPos[0],plPos[1]);
        
        
        //wide bezier
        for (int i = 0; i <numBeziers;i++) {
            Bezier* bez = beziers[(i+bezierNum)%numBeziers];
            int size = sizeof(float)*bezierApproxSize*4;
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(size*i), size,&(bez->approxSidePoints));
        }
        glUniform4f(colorLocation, 0.35f,0.45f,0.5f,1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP,0,(bezierApproxSize*2)*numBeziers);

        //draw bezier
        for (int i = 0; i <numBeziers;i++) {
            Bezier* bez = beziers[(i+bezierNum)%numBeziers];    
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(sizeof(float)*bezierApproxSize*2*i), sizeof(float)*bezierApproxSize*2,&(bez->approxPoints));
        }
        glUniform4f(colorLocation, 1.0f,1.0f,1.0f,0.9f);
        glDrawArrays(GL_LINE_STRIP,0,bezierApproxSize*numBeziers);
        
        //draw target
        glUniform2f(camPosLocation, targetx,targety);
        glUniform4f(colorLocation, 0.65f,0.95f,0.65f,0.2f);
        
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(trifanArray), &trifanArray);
        glDrawArrays(GL_TRIANGLE_FAN,0,trifanSize);
        
        //draw killer
        glUniform2f(camPosLocation, killerx,killery);
        glUniform4f(colorLocation, 0.85f,0.55f,0.25f,0.2f);
        
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(trifanArray), &trifanArray);
        glDrawArrays(GL_TRIANGLE_FAN,0,trifanSize);
        
        //draw coin
        glUniform2f(camPosLocation, aCoinx,aCoiny);
        glUniform4f(colorLocation, 0.85f,0.85f,0.45f,0.95f);
        
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(trifan2Array), &trifan2Array);
        glDrawArrays(GL_TRIANGLE_FAN,0,trifan2Size);
        
        
        glUniform2f(camPosLocation, plPos[0],plPos[1]);
        //player trail
        glUniform4f(colorLocation, 0.95f,0.95f,0.95f,0.1f);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(plTrailVals),plTrailVals);
        glDrawArrays(GL_TRIANGLES,0,plTrailLength*3);
            
        //draw player
        
        if (plAlive) {
            
            float sina = sin(plAngle);
            float cosa = cos(plAngle);
            
            for (int i = 0; i < 6; i+=2) {
                adjPlVerts[i] = plVerts[i]*cosa - plVerts[i+1]*sina + plPos[0];
                adjPlVerts[i+1] = plVerts[i]*sina + plVerts[i+1]*cosa + plPos[1];
                plTrailVals[i+plTrailNum]=adjPlVerts[i];
                plTrailVals[i+plTrailNum+1]=adjPlVerts[i+1];
            }
            plTrailNum=(plTrailNum+6)%plTrailSize;
            
            if (phaseShift) {
                glUniform4f(colorLocation, 0.95f,0.95f,0.95f,0.1f);
            } else {
                glUniform4f(colorLocation, 0.95f,0.95f,0.95f,1.0f);
            }
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(adjPlVerts),&adjPlVerts);
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        
        glUniform2f(camPosLocation,0.0f,0.0f);
        if (!plAlive) {
            glUniform4fv(colorLocation,1,explosion1Colors);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof((explosion1Verts)),&explosion1Verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,sizeof(explosion1Elems),&explosion1Elems);
            glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, 0);
            
            
            glUniform4fv(colorLocation,1,explosion2Colors);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(explosion2Verts),&explosion2Verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,sizeof(explosion2Elems),explosion2Elems);
            glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_BYTE, 0);
        }
        
        //draw score
        glUniform4f(colorLocation, 0.95f,0.95f,0.95f,1.0f);
        
        
        int d = coinCount % 10;
        int c = coinCount / 10 % 10;
        int b = coinCount / 100 % 10;
        int a = coinCount / 1000 % 10;
        
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(numberVerts),&numberVerts);
        
        glUniform2f(camPosLocation,0.5f*(float)windowWidth - 0.7f*numberSize,-0.5f*(float)windowHeight + 1.2f*numberSize);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,numbersElemsLengths[a]*sizeof(float),&numbersElems[a]);
        glDrawElements(GL_LINE_STRIP, numbersElemsLengths[a], GL_UNSIGNED_BYTE, 0);
        
        glUniform2f(camPosLocation,0.5f*(float)windowWidth - 2.0f*numberSize,-0.5f*(float)windowHeight + 1.2f*numberSize);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,numbersElemsLengths[b]*sizeof(float),&numbersElems[b]);
        glDrawElements(GL_LINE_STRIP, numbersElemsLengths[b], GL_UNSIGNED_BYTE, 0);
        
        glUniform2f(camPosLocation,0.5f*(float)windowWidth - 3.3f*numberSize,-0.5f*(float)windowHeight + 1.2f*numberSize);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,numbersElemsLengths[c]*sizeof(float),&numbersElems[c]);
        glDrawElements(GL_LINE_STRIP, numbersElemsLengths[c], GL_UNSIGNED_BYTE, 0);
        
        glUniform2f(camPosLocation,0.5f*(float)windowWidth - 4.6f*numberSize,-0.5f*(float)windowHeight + 1.2f*numberSize);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,numbersElemsLengths[d]*sizeof(float),&numbersElems[d]);
        glDrawElements(GL_LINE_STRIP, numbersElemsLengths[d], GL_UNSIGNED_BYTE, 0);


        
        
        
        glfwSwapBuffers(window);
        
        //error test
        GLenum e = glGetError();
        if (e) {
            printf(getGLErrorStr(e));
            fflush(stdout);
        }
        
        usleep(1000000/100); //100fps
    }
    
    printf("bye\n");
    
    return 0;
}