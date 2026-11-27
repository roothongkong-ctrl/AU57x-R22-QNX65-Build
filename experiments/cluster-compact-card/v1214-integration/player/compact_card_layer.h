// MIB Cluster Map v1.2.14, experimental original-card RGBA GPU layer.
// NO INSTALL. Optional: disabled unless CLUSTERMAP_CARD_INPUT is set.
// Input: 'CCAR' + LE32 width,height,dest_x,dest_y + LE64 CLOCK_MONOTONIC us
//        + prematted straight-alpha RGBA (w*h*4). Atomic rename by producer.
#pragma once
namespace compact_card_layer {
static const char* input_path=NULL;
static GLuint tex=0,prog=0;
static GLint pos=-1,uv=-1,sampler=-1;
static unsigned char* pixels=NULL;
static size_t pixel_capacity=0;
static bool enabled=false;
static const char* vertex=
 "attribute vec2 p; attribute vec2 uv; varying vec2 v;\n"
 "void main() { v=uv; gl_Position=vec4(p,0.,1.); }\n";
static const char* fragment=
 "precision mediump float; varying vec2 v; uniform sampler2D t;\n"
 "void main() { gl_FragColor=texture2D(t,v); }\n";
static uint32_t read32(const unsigned char* p){
 return ((uint32_t)p[0])|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t read64(const unsigned char* p){
 return (uint64_t)read32(p)|((uint64_t)read32(p+4)<<32);
}
static void init(){
 const char* p=getenv("CLUSTERMAP_CARD_INPUT");
 if(!p||strncmp(p,"/tmp/",5)!=0||strlen(p)>220||strlen(p)<10)return;
 input_path=p;
 GLuint vs=glCreateShader(GL_VERTEX_SHADER);
 glShaderSource(vs,1,&vertex,NULL);glCompileShader(vs);
 GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
 glShaderSource(fs,1,&fragment,NULL);glCompileShader(fs);
 GLint ok=0;
 glGetShaderiv(vs,GL_COMPILE_STATUS,&ok);
 if(!ok){LOGE("compact-card: vertex shader failed");return;}
 glGetShaderiv(fs,GL_COMPILE_STATUS,&ok);
 if(!ok){LOGE("compact-card: fragment shader failed");return;}
 prog=glCreateProgram();glAttachShader(prog,vs);glAttachShader(prog,fs);
 glLinkProgram(prog);glGetProgramiv(prog,GL_LINK_STATUS,&ok);
 if(!ok){LOGE("compact-card: program link failed");return;}
 pos=glGetAttribLocation(prog,"p");uv=glGetAttribLocation(prog,"uv");
 sampler=glGetUniformLocation(prog,"t");
 if(pos<0||uv<0||sampler<0){LOGE("compact-card: attributes absent");return;}
 glGenTextures(1,&tex);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,tex);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glActiveTexture(GL_TEXTURE0);
 enabled=true;LOG("compact-card: opt-in layer ready");
}
static void draw(int screenW,int screenH,uint64_t nowUs){
 if(!enabled||screenW<=0||screenH<=0||splash_active())return;
 FILE* f=fopen(input_path,"rb");if(!f)return;
 unsigned char hdr[28];
 if(fread(hdr,1,sizeof(hdr),f)!=sizeof(hdr)||memcmp(hdr,"CCAR",4)!=0){fclose(f);return;}
 uint32_t w=read32(hdr+4),h=read32(hdr+8),x=read32(hdr+12),y=read32(hdr+16);
 uint64_t ts=read64(hdr+20);
 if(w==0||h==0||w>1024||h>512||w>(uint32_t)screenW||h>(uint32_t)screenH||
    x>=(uint32_t)screenW||y>=(uint32_t)screenH||
    ts>nowUs||nowUs-ts>400000ULL){fclose(f);return;}
 size_t n=(size_t)w*h*4;
 if(pixel_capacity<n){unsigned char* p=(unsigned char*)realloc(pixels,n);
  if(!p){fclose(f);return;}pixels=p;pixel_capacity=n;}
 if(fread(pixels,1,n,f)!=n||fgetc(f)!=EOF){fclose(f);return;}
 fclose(f);
 int dw=((int)w*70+50)/100,dh=((int)h*70+50)/100;
 float l=(float)x/screenW*2.f-1.f,rt=(float)((int)x+dw)/screenW*2.f-1.f;
 float top=1.f-(float)y/screenH*2.f,bt=1.f-(float)((int)y+dh)/screenH*2.f;
 const GLfloat xy[8]={l,top,rt,top,rt,bt,l,bt};
 const GLfloat uvxy[8]={0,1,1,1,1,0,0,0};
 glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,tex);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)w,(GLsizei)h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
 glUseProgram(prog);glUniform1i(sampler,3);glEnable(GL_BLEND);
 glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glEnableVertexAttribArray(pos);glEnableVertexAttribArray(uv);
 glVertexAttribPointer(pos,2,GL_FLOAT,GL_FALSE,0,xy);
 glVertexAttribPointer(uv,2,GL_FLOAT,GL_FALSE,0,uvxy);
 glDrawArrays(GL_TRIANGLE_FAN,0,4);
 glDisableVertexAttribArray(pos);glDisableVertexAttribArray(uv);
 glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE0);
}
}
