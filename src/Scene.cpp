#include "Scene.h"

#include "Exception.h"
#include "Matrix4.h"
#include "NV12Shader.h"
#include "OpenGL.h"

//Draws a series of triangles (three-sided polygons) using vertices v0, v1, v2, then v2, v1, v3 (note the order), then v2, v3, v4, and so on.
const float Scene::quadx[]
{
    /* FRONT */
    -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,

    -1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,

    /* BACK */
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

    1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,

    /* LEFT */
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    /* RIGHT */
    1.0f, -1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,

    1.0f, -1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,

    /* TOP */
    -1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,

    /* BOTTOM */
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,

    1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f
};

/** Texture coordinates for the quad. */
const float Scene::texCoords[]
{
    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,


    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,


    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,


    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,


    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f,


    0.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  0.0f,

    1.0f,  0.0f,
    0.0f,  1.0f,
    1.0f,  1.0f

};


const float Scene::quad[] =
{
    -1,  1, 0,
    -1, -1, 0,
    1, -1, 0,

    1, -1, 0,
    1,  1, 0,
    -1,  1, 0
};

const float Scene::quadUV[] =
{
    0, 0,
    0, 1,
    1, 1,

    1, 1,
    1, 0,
    0, 0
};



void Scene::Load()
{
    GLuint nv12Program = NV12Shader::Create();


    glEnableVertexAttribArray(0);
    OpenGL::CheckError();

    glBindAttribLocation(nv12Program, 0, "Attr_Position");
    OpenGL::CheckError();


    glEnableVertexAttribArray(1);
    OpenGL::CheckError();

    glBindAttribLocation(nv12Program, 1, "Attr_TexCoord0");
    OpenGL::CheckError();


    glLinkProgram(nv12Program);
    OpenGL::CheckError();

    glUseProgram(nv12Program);
    OpenGL::CheckError();


    // Create the texture handle

    GLuint textures[2];
    glGenTextures(2, textures);
    OpenGL::CheckError();

    yTexture = textures[0];
    vuTexture = textures[1];


    // Set shader textures
    GLint openGLUniformLocation = glGetUniformLocation(nv12Program, "DiffuseMap");
    OpenGL::CheckError();

    glUniform1i(openGLUniformLocation, 0);
    OpenGL::CheckError();


    openGLUniformLocation = glGetUniformLocation(nv12Program, "VUMap");
    OpenGL::CheckError();

    glUniform1i(openGLUniformLocation, 1);
    OpenGL::CheckError();


    // Get shader uniforms
    wvpUniformLocation = glGetUniformLocation(nv12Program, "WorldViewProjection");
    OpenGL::CheckError();

    if (wvpUniformLocation < 0)
        throw Exception();


    textureScaleUniform = glGetUniformLocation(nv12Program, "TextureScale");
    OpenGL::CheckError();


    textureOffsetUniform = glGetUniformLocation(nv12Program, "TextureOffset");
    OpenGL::CheckError();



    // Set render state
    glEnable(GL_CULL_FACE);
    OpenGL::CheckError();

    glCullFace(GL_BACK);
    OpenGL::CheckError();

    glFrontFace(GL_CCW);
    OpenGL::CheckError();

}

void Scene::CreateTextures(int width, int height, int cropX, int cropY, int cropWidth, int cropHeight)
{
    if (width < 1 || height < 1)
        throw Exception ("Invalid texture size.");

    if (cropX < 0 || cropY < 0 || cropWidth < 1 || cropHeight < 1)
        throw Exception ("Invalid crop.");


    this->width = width;
    this->height = height;

    // Create the textures and reserve memory
    glActiveTexture(GL_TEXTURE0);
    OpenGL::CheckError();

    glBindTexture(GL_TEXTURE_2D, yTexture);
    OpenGL::CheckError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    OpenGL::CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    OpenGL::CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    OpenGL::CheckError();


    glActiveTexture(GL_TEXTURE1);
    OpenGL::CheckError();

    glBindTexture(GL_TEXTURE_2D, vuTexture);
    OpenGL::CheckError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width / 2, height / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, nullptr);
    OpenGL::CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    OpenGL::CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    OpenGL::CheckError();


    // Set texture crop
    float scaleX = (float)cropWidth / (float)width;
    float scaleY = (float)cropHeight / (float)height;

    glUniform2f(textureScaleUniform, scaleX, scaleY);
    OpenGL::CheckError();


    float offsetX = (float)cropX / (float) width;
    float offsetY = (float)cropY / (float) height;

    glUniform2f(textureOffsetUniform, offsetX, offsetY);
    OpenGL::CheckError();

}

void Scene::Draw(void* yData, void* vuData)
{
    if (yData == nullptr || vuData == nullptr)
        throw Exception("Invalid arguments.");

    if (width < 1 || height < 1)
        throw Exception ("Invalid texture state.");


    // Upload texture data
    glActiveTexture(GL_TEXTURE0);
    OpenGL::CheckError();

    glBindTexture(GL_TEXTURE_2D, yTexture);
    OpenGL::CheckError();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, yData);
    OpenGL::CheckError();


    glActiveTexture(GL_TEXTURE1);
    OpenGL::CheckError();

    glBindTexture(GL_TEXTURE_2D, vuTexture);
    OpenGL::CheckError();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2 , GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, vuData);
    OpenGL::CheckError();



    // Clear the render buffer
    glClearColor(0.25f, 0.25f, 0.25f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    OpenGL::CheckError();


    //GLES2.glDrawArrays(GLES2.GL_TRIANGLES, 0, 6 * 4);
    //OpenGL.CheckError();

    if (sceneMode == SceneMode::Cube)
    {
        // Set the vertex data
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * 4, quadx);
        OpenGL::CheckError();

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * 4, texCoords);
        OpenGL::CheckError();


        const float TwoPI = (float)(M_PI * 2.0);

        //rotX += incX;
        rotY += 0.05f;

        while (rotY > TwoPI)
        {
            rotY -= TwoPI;
        }

        rotX += 0.05f;
        while(rotX > TwoPI)
        {
            rotX -= TwoPI;
        }

        Matrix4 world = Matrix4::CreateRotationX(rotX) * Matrix4::CreateRotationY(rotY) * Matrix4::CreateRotationZ(rotZ);
        Matrix4 view = Matrix4::CreateLookAt(Vector3(0, 0, 2.5f), Vector3::Forward, Vector3::Up);

        Matrix4 wvp = world * view * Matrix4::CreatePerspectiveFieldOfView(M_PI_4, 16.0f / 9.0f, 0.1f, 50);

        Matrix4 transpose = Matrix4::CreateTranspose((wvp));
        float* wvpValues = &transpose.M11;


        glUniformMatrix4fv(wvpUniformLocation, 1, GL_FALSE, wvpValues);
        OpenGL::CheckError();


        glDrawArrays(GL_TRIANGLES, 0, 3 * 2 * 6);
        OpenGL::CheckError();
    }
    else if (sceneMode == SceneMode::Quad)
    {
        // Set the vertex data
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * 4, quad);
        OpenGL::CheckError();

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * 4, quadUV);
        OpenGL::CheckError();


        // Set the matrix
        Matrix4 transpose = Matrix4::CreateTranspose(Matrix4::Identity);
        float* wvpValues = &transpose.M11;

        glUniformMatrix4fv(wvpUniformLocation, 1, GL_FALSE, wvpValues);
        OpenGL::CheckError();


        // Draw
        glDrawArrays(GL_TRIANGLES, 0, 3 * 2);
        OpenGL::CheckError();
    }
    else
    {
        throw Exception("Unsupported sceneMode");
    }
}
