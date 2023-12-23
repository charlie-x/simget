#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <iostream>

class FrameBuffer
{
public:
    FrameBuffer(GLsizei width, GLsizei height);
    ~FrameBuffer();
    unsigned int getFrameTexture() const;
    void RescaleFrameBuffer(GLsizei newWidth, GLsizei newHeight);
    void Bind() const;
    void Unbind() const;

private:
    unsigned int fbo;
    unsigned int texture;
    unsigned int rbo;
    GLsizei width, height; // Store the current dimensions of the framebuffer
};

#endif // FRAMEBUFFER_H


