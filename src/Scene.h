#pragma once


#include <GLES2/gl2.h>
enum class SceneMode
{
    Cube = 0,
    Quad
};


class Scene
{
    GLuint yTexture = 0;
    GLuint vuTexture = 0;
    int width = 0;
    int height = 0;
    int cropX;
    int cropY;
    int cropWidth;
    int cropHeight;
    GLint wvpUniformLocation = -1;
    GLint textureScaleUniform = -1;
    GLint textureOffsetUniform = -1;
    float rotX = 0;
    float rotY = 0;
    float rotZ = 0;
    SceneMode sceneMode = SceneMode::Quad; //SceneMode::Cube;

    static const float quadx[];
    static const float texCoords[];

    static const float quad[];
    static const float quadUV[];

public:

    SceneMode GetSceneMode() const
    {
        return sceneMode;
    }
    void SetSceneMode(SceneMode value)
    {
        sceneMode = value;
    }


    Scene()
    {
    }


    void Load();

    void CreateTextures(int width, int height, int cropX, int cropY, int cropWidth, int cropHeight);

    void Draw(void* yData, void* vuData);

};

