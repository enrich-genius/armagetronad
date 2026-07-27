/*
 * Fixed-function GL entry points that Armagetron uses but emscripten's
 * LEGACY_GL_EMULATION does not provide.
 *
 *  - glRectf / glTexCoord3fv : expressed via primitives emscripten does supply.
 *  - display lists           : emscripten has no display lists at all. The game
 *                              keeps sr_useDisplayLists == rDisplayList_Off on
 *                              this target, so these are never exercised at
 *                              run time; they exist only to satisfy the linker.
 */
#include <GL/gl.h>

void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    /* A rectangle drawn as a triangle fan (GL_QUADS/GL_POLYGON may be absent
       from the emulation, but a 4-vertex convex fan is always fine). */
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

/* Not via glTexCoord3f: emscripten declares that one and aborts inside it, so
   this shim used to link cleanly and then kill the tab on the first textured
   model face. The third coordinate is ignored by fixed function 2D texturing,
   which is all this target uses. */
void glTexCoord3fv(const GLfloat *v)
{
    glTexCoord2f(v[0], v[1]);
}

void glTexCoord2d(GLdouble s, GLdouble t)
{
    glTexCoord2f((GLfloat)s, (GLfloat)t);
}

/* ---- display list no-ops (unused at run time on this target) ------------- */
GLuint glGenLists(GLsizei range)                 { (void)range; return 0; }
void   glNewList(GLuint list, GLenum mode)       { (void)list; (void)mode; }
void   glEndList(void)                           { }
void   glCallList(GLuint list)                   { (void)list; }
void   glDeleteLists(GLuint list, GLsizei range) { (void)list; (void)range; }
