// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "Annotation.h"

#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QImage>
#include <QPainter>

/// Creates and returns a texture containing some text.
static std::unique_ptr<Texture> makeTextureFromText(const QString& text)
{
    QFont font("Sans", 13);
    // Text outline makes the text thinner so we have to bold it
    font.setWeight(QFont::DemiBold);
    QFontMetrics fm(font);
    int width = fm.width(text);
    int height = fm.ascent() + fm.descent();
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(0, height + fm.descent());
    painter.scale(1, -1);
    painter.setBrush(Qt::white);
    // Add a black outline for visibility when on a white background
    QPen pen;
    pen.setWidthF(1.2);
    pen.setColor(Qt::black);
    painter.setPen(pen);
    QPainterPath path;
    path.addText(0, height, font, text);
    painter.drawPath(path);
    std::unique_ptr<Texture> texture(new Texture(image));
    // The text is blurry when using GL_LINEAR. I suspect this is a result of
    // floating point inaccuracies in the vertex shader.
    texture->setResizeFilter(GL_NEAREST);
    return texture;
}

/// Creates a texturable rectangle VAO and returns it's name.
static GLuint makeVAO(GLuint annotationShaderProg)
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Setup buffer
    GlBuffer buffer;
    buffer.bind(GL_ARRAY_BUFFER);
    float data[] = { // x,  y, s, t
                       -1, -1, 0, 0, // bottom-left
                        1, -1, 1, 0, // bottom-right
                       -1,  1, 0, 1, // top-left
                        1,  1, 1, 1, }; // top-right
    size_t rowSize = sizeof (float) * 4;
    size_t xyzOffset = 0;
    size_t stOffset = sizeof (float) * 2;
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);

    // Setup attributes
    GLint positionAttribute = glGetAttribLocation(annotationShaderProg, "position");
    glVertexAttribPointer(positionAttribute,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          rowSize,
                          (void*) xyzOffset);
    glEnableVertexAttribArray(positionAttribute);

    GLint texCoordAttribute = glGetAttribLocation(annotationShaderProg, "texCoord");
    glVertexAttribPointer(texCoordAttribute,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          rowSize,
                          (void*) stOffset);
    glEnableVertexAttribArray(texCoordAttribute);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

Annotation::Annotation(const QString& label,
                       GLuint annotationShaderProg,
                       const QString& text,
                       Imath::V3d position)
     : m_label(label),
     m_text(text),
     m_position(position),
     m_texture(makeTextureFromText(text)),
     m_vao(makeVAO(annotationShaderProg))
{
}

Annotation::~Annotation()
{
    glDeleteVertexArrays(1, &m_vao);
}

void Annotation::draw(QGLShaderProgram& annotationShaderProg,
                      const TransformState& transState) const
{
    glBindVertexArray(m_vao);
    GLint texture0 = glGetUniformLocation(annotationShaderProg.programId(),
                                          "texture0");
    m_texture->bind(texture0);
    annotationShaderProg.setUniformValue("annotationSize",
                                        m_texture->width(),
                                        m_texture->height());
    transState.translate(m_position)
              .setUniforms(annotationShaderProg.programId());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}
