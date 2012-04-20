#include <stdio.h>

#ifdef WIN32
#define NOMINMAX
#endif

#include <GL/glew.h>
#ifdef WIN32
#include <GL/wglew.h>
#else
#include <GL/glxew.h>
#endif

#if defined(__APPLE__) || defined(MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif

#include <cuda_runtime_api.h>

#include "renderloop.h"
#include "render_particles.h"
#include "SmokeRenderer.h"
#include "vector_math.h"

void drawWireBox(float3 boxMin, float3 boxMax) {
  glLineWidth(1.0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  
  glColor3f(0.0, 1.0, 0.0);
  glBegin(GL_QUADS);
    // Front Face
    glNormal3f( 0.0, 0.0, 1.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMin.x, boxMin.y, boxMax.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMax.x, boxMin.y, boxMax.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMax.x, boxMax.y, boxMax.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMin.x, boxMax.y, boxMax.z);
    // Back Face
    glNormal3f( 0.0, 0.0,-1.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMax.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMin.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMin.x, boxMax.y, boxMin.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMax.x, boxMax.y, boxMin.z);
    // Top Face
    glNormal3f( 0.0, 1.0, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMin.x, boxMax.y, boxMax.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMax.x, boxMax.y, boxMax.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMax.x, boxMax.y, boxMin.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMin.x, boxMax.y, boxMin.z);
    // Bottom Face
    glNormal3f( 0.0,-1.0, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMax.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMin.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMin.x, boxMin.y, boxMax.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMax.x, boxMin.y, boxMax.z);
    // Right face
    glNormal3f( 1.0, 0.0, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMax.x, boxMin.y, boxMax.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMax.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMax.x, boxMax.y, boxMin.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMax.x, boxMax.y, boxMax.z);
    // Left Face
    glNormal3f(-1.0, 0.0, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(boxMin.x, boxMin.y, boxMin.z);
    glTexCoord2f(1.0, 0.0); glVertex3f(boxMin.x, boxMin.y, boxMax.z);
    glTexCoord2f(1.0, 1.0); glVertex3f(boxMin.x, boxMax.y, boxMax.z);
    glTexCoord2f(0.0, 1.0); glVertex3f(boxMin.x, boxMax.y, boxMin.z);
  glEnd();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  
}

class BonsaiDemo
{
public:
  BonsaiDemo(octree *tree, octree::IterationData &idata) 
    : m_tree(tree), m_idata(idata), iterationsRemaining(true),
      m_renderer(tree->localTree.n),
      //m_displayMode(ParticleRenderer::PARTICLE_SPRITES_COLOR),
	  m_displayMode(SmokeRenderer::VOLUMETRIC),
      m_ox(0), m_oy(0), m_buttonState(0), m_inertia(0.1f),
      m_paused(false),
  	  m_displayBoxes(false), 
	    m_displaySliders(false),
	    m_enableGlow(true),
	    m_displayLightBuffer(false),
      m_octreeDisplayLevel(3),
	    m_fov(60.0f)
  {
    m_windowDims = make_int2(1024, 768);
    m_cameraTrans = make_float3(0, -2, -100);
    m_cameraTransLag = m_cameraTrans;
    m_cameraRot = make_float3(0, 0, 0);
    m_cameraRotLag = m_cameraRot;
            
    //float color[4] = { 0.8f, 0.7f, 0.95f, 0.5f};
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
    //m_renderer.setBaseColor(color);
    //m_renderer.setPointSize(0.00001f);
    tree->iterate_setup(m_idata);

	m_particleColors = new float4[tree->localTree.n];

	m_renderer.setFOV(m_fov);
	m_renderer.setWindowSize(m_windowDims.x, m_windowDims.y);
	m_renderer.setDisplayMode(m_displayMode);
  }

  ~BonsaiDemo() {
    m_tree->iterate_teardown(m_idata);
    delete m_tree;
	delete [] m_particleColors;
  }

  void cycleDisplayMode() {
    //m_displayMode = (ParticleRenderer::DisplayMode) ((m_displayMode + 1) % ParticleRenderer::PARTICLE_NUM_MODES);
	  m_displayMode = (SmokeRenderer::DisplayMode) ((m_displayMode + 1) % SmokeRenderer::NUM_MODES);
	  m_renderer.setDisplayMode(m_displayMode);
    // MJH todo: add body color support and remove this
    //if (ParticleRenderer::PARTICLE_SPRITES_COLOR == m_displayMode)
    //  cycleDisplayMode();
  }

  void togglePause() { m_paused = !m_paused; }
  void toggleBoxes() { m_displayBoxes = !m_displayBoxes; }
  void toggleSliders() { m_displaySliders = !m_displaySliders; }
  void toggleGlow() { m_enableGlow = !m_enableGlow; m_renderer.setEnableFilters(m_enableGlow); }
  void toggleLightBuffer() { m_displayLightBuffer = !m_displayLightBuffer; m_renderer.setDisplayLightBuffer(m_displayLightBuffer); }

  void incrementOctreeDisplayLevel(int inc) { 
    m_octreeDisplayLevel += inc;
    m_octreeDisplayLevel = std::max(0, std::min(m_octreeDisplayLevel, 30));
  }

  void step() { 
    if (!m_paused && iterationsRemaining)
      iterationsRemaining = !m_tree->iterate_once(m_idata); 
    if (!iterationsRemaining)
      printf("No iterations Remaining!\n");
  }

  void display() { 
    getBodyData();

    // view transform
    {
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

      m_cameraTransLag += (m_cameraTrans - m_cameraTransLag) * m_inertia;
      m_cameraRotLag += (m_cameraRot - m_cameraRotLag) * m_inertia;
      
      glTranslatef(m_cameraTransLag.x, m_cameraTransLag.y, m_cameraTransLag.z);
      glRotatef(m_cameraRotLag.x, 1.0, 0.0, 0.0);
      glRotatef(m_cameraRotLag.y, 0.0, 1.0, 0.0);
    }

    //m_renderer.display(m_displayMode);
	  m_renderer.render();

    if (m_displayBoxes) {
      glEnable(GL_DEPTH_TEST);
      displayOctree();  
    }

	if (m_displaySliders) {
		m_renderer.getParams()->Render(0, 0);
	}
  }

  void mouse(int button, int state, int x, int y)
  {
    int mods;

	if (m_displaySliders) {
		if (m_renderer.getParams()->Mouse(x, y, button, state))
			return;
	}

    if (state == GLUT_DOWN) {
        m_buttonState |= 1<<button;
    }
    else if (state == GLUT_UP) {
        m_buttonState = 0;
    }

    mods = glutGetModifiers();

    if (mods & GLUT_ACTIVE_SHIFT) {
        m_buttonState = 2;
    }
    else if (mods & GLUT_ACTIVE_CTRL) {
        m_buttonState = 3;
    }

    m_ox = x;
    m_oy = y;
  }

  void motion(int x, int y)
  {
    float dx = (float)(x - m_ox);
    float dy = (float)(y - m_oy);

	if (m_displaySliders) {
	  if (m_renderer.getParams()->Motion(x, y))
		return;
	}

    if (m_buttonState == 3) {
      // left+middle = zoom
      m_cameraTrans.z += (dy / 100.0f) * 0.5f * fabs(m_cameraTrans.z);
    }
    else if (m_buttonState & 2) {
      // middle = translate
      m_cameraTrans.x += dx / 10.0f;
      m_cameraTrans.y -= dy / 10.0f;
    }
    else if (m_buttonState & 1) {
      // left = rotate
      m_cameraRot.x += dy / 5.0f;
      m_cameraRot.y += dx / 5.0f;
    }

    m_ox = x;
    m_oy = y;
  }

  void reshape(int w, int h) {
    m_windowDims = make_int2(w, h);

	m_renderer.setFOV(m_fov);
	m_renderer.setWindowSize(m_windowDims.x, m_windowDims.y);

    fitCamera();
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, m_windowDims.x, m_windowDims.y);
  }

  void fitCamera() {
    float3 boxMin = make_float3(m_tree->rMinLocalTree);
    float3 boxMax = make_float3(m_tree->rMaxLocalTree);

    const float pi = 3.1415926f;
    float3 center = 0.5f * (boxMin + boxMax);
    float radius = std::max(length(boxMax), length(boxMin));
    const float fovRads = (m_windowDims.x / (float)m_windowDims.y) * pi / 3.0f ; // 60 degrees

    float distanceToCenter = radius / sinf(0.5f * fovRads);
    
    m_cameraTrans = center + make_float3(0, 0, - distanceToCenter);
	m_cameraTransLag = m_cameraTrans;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(m_fov, 
                   (float) m_windowDims.x / (float) m_windowDims.y, 
                   0.0001 * distanceToCenter, 
                   4 * (radius + distanceToCenter));
  }

  ParamListGL *getParams() { return m_renderer.getParams(); }

private:
  float  frand() { return rand() / (float) RAND_MAX; }
  float4 randColor(float scale) { return make_float4(frand()*scale, frand()*scale, frand()*scale, 0.0f); }

  void getBodyData() {
    m_tree->localTree.bodies_pos.d2h();
    m_tree->localTree.bodies_ids.d2h();
    //m_tree->localTree.bodies_vel.d2h();

    int n = m_tree->localTree.n;

    //float4 starColor = make_float4(1.0f, 0.75f, 0.1f, 1.0f);	// yellowish
	float4 starColor = make_float4(1.0f, 1.0f, 1.0f, 1.0f);		// white
	float4 starColor2 = make_float4(1.0f, 0.1f, 0.5f, 1.0f) * make_float4(20.0f, 20.0f, 20.0f, 1.0f);		// purplish

	float overbright = 1.0f;
	starColor *= make_float4(overbright, overbright, overbright, 1.0f);

	float4 dustColor = make_float4(0.0f, 0.1f, 0.25f, 0.0f);	// blue
	//float4 dustColor = 	make_float4(0.1f, 0.1f, 0.1f, 0.0f);	// grey

    //float4 *colors = new float4[n];
	float4 *colors = m_particleColors;

    for (int i = 0; i < n; i++) {
      int id = m_tree->localTree.bodies_ids[i];
	  //printf("%d: id %d, mass: %f\n", i, id, m_tree->localTree.bodies_pos[i].w);
	  srand(id*1783);
#if 1
	  float r = frand();
	  if (id >= 0 && id < 100000000) {
		// dust -- not used yet
		colors[i] = make_float4(1, 0, 0, 1);
	  } else if (id >= 100000000 && id < 200000000) {
	    // dark matter
//         colors[i] = starColor + randColor(0.1f);
//         colors[i] = starColor; // * powf(r, 2.0f);
		  colors[i] = (frand() < 0.99f) ? starColor : starColor2;
	  } else {
		// stars
		//colors[i] = dustColor * make_float4(r, r, r, 1.0f);
		colors[i] = dustColor;
	  }
#else
	  // test sorting
	  colors[i] = make_float4(frand(), frand(), frand(), 1.0f);
#endif
    }

    //m_renderer.setPositions((float*)&m_tree->localTree.bodies_pos[0], n);
    //m_renderer.setColors((float*)colors, n);
	m_renderer.setNumParticles(n);
	m_renderer.setPositions((float*)&m_tree->localTree.bodies_pos[0]);
	m_renderer.setColors((float*)colors);

    //delete [] colors;
  }

  void displayOctree() {
    float3 boxMin = make_float3(m_tree->rMinLocalTree);
    float3 boxMax = make_float3(m_tree->rMaxLocalTree);

    drawWireBox(boxMin, boxMax);
      
    m_tree->localTree.boxCenterInfo.d2h();
    m_tree->localTree.boxSizeInfo.d2h();
    m_tree->localTree.node_level_list.d2h(); //Should not be needed is created on host
           
    int displayLevel = min(m_octreeDisplayLevel, m_tree->localTree.n_levels);
      
    for(uint i=0; i < m_tree->localTree.level_list[displayLevel].y; i++)
    {
      float3 boxMin, boxMax;
      boxMin.x = m_tree->localTree.boxCenterInfo[i].x-m_tree->localTree.boxSizeInfo[i].x;
      boxMin.y = m_tree->localTree.boxCenterInfo[i].y-m_tree->localTree.boxSizeInfo[i].y;
      boxMin.z = m_tree->localTree.boxCenterInfo[i].z-m_tree->localTree.boxSizeInfo[i].z;

      boxMax.x = m_tree->localTree.boxCenterInfo[i].x+m_tree->localTree.boxSizeInfo[i].x;
      boxMax.y = m_tree->localTree.boxCenterInfo[i].y+m_tree->localTree.boxSizeInfo[i].y;
      boxMax.z = m_tree->localTree.boxCenterInfo[i].z+m_tree->localTree.boxSizeInfo[i].z;
      drawWireBox(boxMin, boxMax);
    }
  }

  void displayOctree() {
    float3 boxMin = make_float3(m_tree->rMinLocalTree);
    float3 boxMax = make_float3(m_tree->rMaxLocalTree);

    drawWireBox(boxMin, boxMax);
      
    m_tree->localTree.boxCenterInfo.d2h();
    m_tree->localTree.boxSizeInfo.d2h();
    m_tree->localTree.node_level_list.d2h(); //Should not be needed is created on host
           
    int displayLevel = min(m_octreeDisplayLevel, m_tree->localTree.n_levels);
      
    for(uint i=0; i < m_tree->localTree.level_list[displayLevel].y; i++)
    {
      float3 boxMin, boxMax;
      boxMin.x = m_tree->localTree.boxCenterInfo[i].x-m_tree->localTree.boxSizeInfo[i].x;
      boxMin.y = m_tree->localTree.boxCenterInfo[i].y-m_tree->localTree.boxSizeInfo[i].y;
      boxMin.z = m_tree->localTree.boxCenterInfo[i].z-m_tree->localTree.boxSizeInfo[i].z;

      boxMax.x = m_tree->localTree.boxCenterInfo[i].x+m_tree->localTree.boxSizeInfo[i].x;
      boxMax.y = m_tree->localTree.boxCenterInfo[i].y+m_tree->localTree.boxSizeInfo[i].y;
      boxMax.z = m_tree->localTree.boxCenterInfo[i].z+m_tree->localTree.boxSizeInfo[i].z;
      drawWireBox(boxMin, boxMax);
    }
  }
  octree *m_tree;
  octree::IterationData &m_idata;
  bool iterationsRemaining;

  //ParticleRenderer m_renderer;
  //ParticleRenderer::DisplayMode m_displayMode; 
  SmokeRenderer m_renderer;
  SmokeRenderer ::DisplayMode m_displayMode; 
  int m_octreeDisplayLevel;

  float4 *m_particleColors;

  // view params
  int m_ox; // = 0
  int m_oy; // = 0;
  int m_buttonState;     
  int2 m_windowDims;
  float m_fov;
  float3 m_cameraTrans;   
  float3 m_cameraRot;     
  float3 m_cameraTransLag;
  float3 m_cameraRotLag;
  const float m_inertia;

  bool m_paused;
  bool m_displayBoxes;
  bool m_displaySliders;
  bool m_enableGlow;
  bool m_displayLightBuffer;
};

BonsaiDemo *theDemo = NULL;

void onexit() {
  if (theDemo) delete theDemo;
}

void display()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  theDemo->step();
  theDemo->display();

  glutSwapBuffers();
}

void reshape(int w, int h)
{
  theDemo->reshape(w, h);
  glutPostRedisplay();
}

void mouse(int button, int state, int x, int y)
{
  theDemo->mouse(button, state, x, y);
  glutPostRedisplay();
}

void motion(int x, int y)
{
  theDemo->motion(x, y);
  glutPostRedisplay();
}

// commented out to remove unused parameter warnings in Linux
void key(unsigned char key, int /*x*/, int /*y*/)
{
  switch (key) {
  case ' ':
    theDemo->togglePause();
    break;
  case 27: // escape
  case 'q':
  case 'Q':
    cudaDeviceReset();
    exit(0);
    break;
  case '`':
     theDemo->toggleSliders();
     break;
  case 'p':
  case 'P':
    theDemo->cycleDisplayMode();
    break;
  case 'b':
  case 'B':
    theDemo->toggleBoxes();
    break;
  case 'd':
  case 'D':
    //displayEnabled = !displayEnabled;
 	theDemo->toggleLightBuffer();
    break;
  case 'f':
  case 'F':
    theDemo->fitCamera();
    break;
  case ',':
  case '<':
    theDemo->incrementOctreeDisplayLevel(-1);
    break;
  case '.':
  case '>':
    theDemo->incrementOctreeDisplayLevel(+1);
    break;
  case 'h':
  	theDemo->toggleSliders();
	  break;
  case 'g':
	  theDemo->toggleGlow();
	  break;
  }

  glutPostRedisplay();
}

void special(int key, int x, int y)
{
	theDemo->getParams()->Special(key, x, y);
    glutPostRedisplay();
}

void idle(void)
{
    glutPostRedisplay();
}

void initGL(int argc, char** argv)
{  
  // First initialize OpenGL context, so we can properly set the GL for CUDA.
  // This is necessary in order to achieve optimal performance with OpenGL/CUDA interop.
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
  //glutInitWindowSize(720, 480);
  glutInitWindowSize(1024, 768);
  glutCreateWindow("Bonsai Tree-code Gravitational N-body Simulation");
  //if (bFullscreen)
  //  glutFullScreen();
  GLenum err = glewInit();

  if (GLEW_OK != err)
  {
    fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
    cudaDeviceReset();
    exit(-1);
  }
  else if (!glewIsSupported("GL_VERSION_2_0 "
    "GL_VERSION_1_5 "
    "GL_ARB_multitexture "
    "GL_ARB_vertex_buffer_object")) 
  {
    fprintf(stderr, "Required OpenGL extensions missing.");
    exit(-1);
  }
  else
  {
#if   defined(WIN32)
    wglSwapIntervalEXT(0);
#elif defined(LINUX)
    glxSwapIntervalSGI(0);
#endif      
  }

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutKeyboardFunc(key);
  glutSpecialFunc(special);
  glutIdleFunc(idle);

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.0, 0.0, 0.0, 1.0);

  checkGLErrors("initGL");

  atexit(onexit);
}


void initAppRenderer(int argc, char** argv, octree *tree, octree::IterationData &idata) {
  initGL(argc, argv);
  theDemo = new BonsaiDemo(tree, idata);
  glutMainLoop();
}