#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <vector>
#include <fstream>
#include <string>

#define LOG_TAG "GTAEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Estado del engine
static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLSurface eglSurface = EGL_NO_SURFACE;
static EGLContext eglContext = EGL_NO_CONTEXT;
static int screenW = 0, screenH = 0;

// Shaders
const char* vertSrc = R"(
#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main(){
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragSrc = R"(
#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){
    fragColor = texture(uTex, vUV);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

GLuint createProgram() {
    GLuint v = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    return p;
}

// Matriz 4x4
struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static Mat4 identity() { return Mat4(); }
};

Mat4 perspective(float fov, float aspect, float zn, float zf) {
    Mat4 m;
    float f = 1.0f / tanf(fov/2);
    m.m[0]=f/aspect; m.m[5]=f;
    m.m[10]=(zf+zn)/(zn-zf); m.m[11]=-1;
    m.m[14]=(2*zf*zn)/(zn-zf); m.m[15]=0;
    return m;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++) {
            r.m[i*4+j]=0;
            for(int k=0;k<4;k++)
                r.m[i*4+j]+=a.m[i*4+k]*b.m[k*4+j];
        }
    return r;
}

// Camera
float camX=90,camY=5,camZ=100;
float yaw=0,pitch=-0.2f;

// Touch
float touchLX=0,touchLY=0,touchRX=0,touchRY=0;
bool touchL=false,touchR=false;

// Map data
struct Vec3{float x,y,z;};
std::vector<Vec3> mapVerts;
struct Chunk{ int cx,cz; std::vector<unsigned int> indices; };
std::vector<Chunk> chunks;
GLuint mapTex=0, mapVBO=0, prog=0;

void loadAssets(const std::string& dataDir) {
    // Cargar verts
    std::ifstream fv(dataDir+"/map_verts.bin",std::ios::binary);
    unsigned int vc; fv.read((char*)&vc,4);
    mapVerts.resize(vc);
    fv.read((char*)mapVerts.data(),vc*12);
    LOGI("Verts: %u", vc);

    // Cargar chunks
    std::ifstream fc(dataDir+"/map_chunks.bin",std::ios::binary);
    unsigned int cc; fc.read((char*)&cc,4);
    chunks.resize(cc);
    for(auto& c:chunks){
        fc.read((char*)&c.cx,4);
        fc.read((char*)&c.cz,4);
        unsigned int ic; fc.read((char*)&ic,4);
        c.indices.resize(ic);
        fc.read((char*)c.indices.data(),ic*4);
    }
    LOGI("Chunks: %u", cc);

    // VBO de verts
    glGenBuffers(1,&mapVBO);
    glBindBuffer(GL_ARRAY_BUFFER,mapVBO);
    glBufferData(GL_ARRAY_BUFFER,mapVerts.size()*12,mapVerts.data(),GL_STATIC_DRAW);
}

void initGL(ANativeWindow* window) {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eglDisplay, nullptr, nullptr);

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,8, EGL_GREEN_SIZE,8, EGL_RED_SIZE,8,
        EGL_DEPTH_SIZE,16, EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay,attribs,&config,1,&numConfigs);

    EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    eglContext = eglCreateContext(eglDisplay,config,EGL_NO_CONTEXT,ctxAttribs);
    eglSurface = eglCreateWindowSurface(eglDisplay,config,window,nullptr);
    eglMakeCurrent(eglDisplay,eglSurface,eglSurface,eglContext);

    eglQuerySurface(eglDisplay,eglSurface,EGL_WIDTH,&screenW);
    eglQuerySurface(eglDisplay,eglSurface,EGL_HEIGHT,&screenH);

    prog = createProgram();
    glEnable(GL_DEPTH_TEST);
    LOGI("GL init OK %dx%d", screenW, screenH);
}

void renderFrame() {
    // Update camera
    if(touchL){
        float dx=(touchLX-screenW*0.25f)/(screenW*0.2f);
        float dy=(touchLY-screenH*0.75f)/(screenH*0.2f);
        camX+=cosf(yaw)*dy*0.3f+sinf(yaw+M_PI/2)*dx*0.3f;
        camZ+=sinf(yaw)*dy*0.3f-cosf(yaw+M_PI/2)*dx*0.3f;
    }
    if(touchR){
        yaw+=(touchRX-screenW*0.75f)/(screenW*2.0f);
        pitch-=(touchRY-screenH*0.75f)/(screenH*2.0f);
        if(pitch>0.8f)pitch=0.8f;
        if(pitch<-0.8f)pitch=-0.8f;
    }

    glViewport(0,0,screenW,screenH);
    glClearColor(0.4f,0.6f,0.9f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog);

    // MVP
    Mat4 proj = perspective(60.0f*M_PI/180.0f,(float)screenW/screenH,0.1f,1000.0f);
    // View simple
    float lx=camX+sinf(yaw)*cosf(pitch);
    float ly=camY+sinf(pitch);
    float lz=camZ-cosf(yaw)*cosf(pitch);
    // TODO: view matrix completa
    Mat4 mvp = proj;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,GL_FALSE,mvp.m);

    // Render chunks cercanos
    int curCX=(int)(camX/18.0f);
    int curCZ=(int)(camZ/18.0f);

    glBindBuffer(GL_ARRAY_BUFFER,mapVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,0);

    for(auto& chunk:chunks){
        if(abs(chunk.cx-curCX)>3||abs(chunk.cz-curCZ)>3) continue;
        GLuint ibo;
        glGenBuffers(1,&ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,chunk.indices.size()*4,chunk.indices.data(),GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES,chunk.indices.size(),GL_UNSIGNED_INT,0);
        glDeleteBuffers(1,&ibo);
    }

    eglSwapBuffers(eglDisplay,eglSurface);
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_gta_engine_GTAEngine_nativeInit(JNIEnv* env, jobject, jobject surface, jstring dataPath) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    initGL(window);
    const char* path = env->GetStringUTFChars(dataPath, nullptr);
    loadAssets(std::string(path));
    env->ReleaseStringUTFChars(dataPath, path);
}

JNIEXPORT void JNICALL
Java_com_gta_engine_GTAEngine_nativeRender(JNIEnv*, jobject) {
    renderFrame();
}

JNIEXPORT void JNICALL
Java_com_gta_engine_GTAEngine_nativeTouch(JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    bool left = x < screenW/2;
    if(action==0){ // DOWN
        if(left){touchL=true;touchLX=x;touchLY=y;}
        else{touchR=true;touchRX=x;touchRY=y;}
    } else if(action==2){ // MOVE
        if(left&&touchL){touchLX=x;touchLY=y;}
        if(!left&&touchR){touchRX=x;touchRY=y;}
    } else { // UP
        if(left)touchL=false; else touchR=false;
    }
}

}
