/*
 * Minimal GLU implementation for the emscripten build.
 *
 * Emscripten's <GL/glu.h> declares the GLU entry points but ships no
 * implementation. Armagetron only uses four of them; we provide those here
 * on top of the fixed-function (LEGACY_GL_EMULATION) matrix stack.
 */
#include <GL/gl.h>
#include <math.h>

/* glGenerateMipmap is a GLES2/GL3 entry point that emscripten's WebGL layer
   exports, but it is not declared by the legacy <GL/gl.h>. */
extern void glGenerateMipmap(GLenum target);

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
    GLfloat m[16];
    double f = 1.0 / tan((fovy * M_PI / 180.0) / 2.0);
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0]  = (GLfloat)(f / aspect);
    m[5]  = (GLfloat)f;
    m[10] = (GLfloat)((zFar + zNear) / (zNear - zFar));
    m[11] = -1.0f;
    m[14] = (GLfloat)((2.0 * zFar * zNear) / (zNear - zFar));
    glMultMatrixf(m);
}

static void normalize3(double v[3])
{
    double len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len == 0.0) return;
    v[0] /= len; v[1] /= len; v[2] /= len;
}

static void cross3(const double a[3], const double b[3], double out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

void gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
               GLdouble centerX, GLdouble centerY, GLdouble centerZ,
               GLdouble upX, GLdouble upY, GLdouble upZ)
{
    double forward[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    double up[3] = { upX, upY, upZ };
    double side[3], up2[3];

    normalize3(forward);
    cross3(forward, up, side);
    normalize3(side);
    cross3(side, forward, up2);

    GLfloat m[16] = {
        (GLfloat)side[0], (GLfloat)up2[0], (GLfloat)-forward[0], 0.0f,
        (GLfloat)side[1], (GLfloat)up2[1], (GLfloat)-forward[1], 0.0f,
        (GLfloat)side[2], (GLfloat)up2[2], (GLfloat)-forward[2], 0.0f,
        0.0f,             0.0f,            0.0f,                 1.0f
    };
    glMultMatrixf(m);
    glTranslatef((GLfloat)-eyeX, (GLfloat)-eyeY, (GLfloat)-eyeZ);
}

GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                        GLsizei width, GLsizei height,
                        GLenum format, GLenum type, const void *data)
{
    /* WebGL can generate mipmaps for us. Upload the base level, then ask the
       driver to build the rest. */
    glTexImage2D(target, 0, internalFormat, width, height, 0, format, type, data);
    glGenerateMipmap(target);
    return 0;
}

const GLubyte *gluErrorString(GLenum error)
{
    switch (error) {
        case GL_NO_ERROR:          return (const GLubyte *)"no error";
        case GL_INVALID_ENUM:      return (const GLubyte *)"invalid enum";
        case GL_INVALID_VALUE:     return (const GLubyte *)"invalid value";
        case GL_INVALID_OPERATION: return (const GLubyte *)"invalid operation";
        case GL_OUT_OF_MEMORY:     return (const GLubyte *)"out of memory";
        default:                   return (const GLubyte *)"unknown error";
    }
}
