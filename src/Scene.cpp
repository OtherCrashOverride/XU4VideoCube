#include "Scene.h"

#include "Exception.h"
#include "Matrix4.h"
#include "NV12Shader.h"
#include "Egl.h"

#include <xf86drm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <drm/drm_fourcc.h>
#include <string.h>

#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/eglext.h>

//#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2ext.h>

#include "X11Window.h"


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
    //vuTexture = textures[1];


    // Set shader textures
    GLint openGLUniformLocation = glGetUniformLocation(nv12Program, "DiffuseMap");
    OpenGL::CheckError();

    glUniform1i(openGLUniformLocation, 0);
    OpenGL::CheckError();


    //openGLUniformLocation = glGetUniformLocation(nv12Program, "VUMap");
    //OpenGL::CheckError();

 /*   glUniform1i(openGLUniformLocation, 1);
    OpenGL::CheckError();*/


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

int Scene::CreateBuffer(int fd, Display* dpy, int width, int height, int bpp)
{
	// Create dumb buffer
	drm_mode_create_dumb buffer = { 0 };
	buffer.width = width;
	buffer.height = height;
	buffer.bpp = bpp;

	int ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &buffer);
	if (ret < 0)
	{
		throw Exception("DRM_IOCTL_MODE_CREATE_DUMB failed.");
	}


	// Get the dmabuf for the buffer
	drm_prime_handle prime = { 0 };
	prime.handle = buffer.handle;
	prime.flags = DRM_CLOEXEC | DRM_RDWR;

	ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
	if (ret < 0)
	{
		throw Exception("DRM_IOCTL_PRIME_HANDLE_TO_FD failed.");
	}


	return prime.fd;
}


void Scene::CreateTextures(int drmfd, Display* dpy, EGLDisplay eglDisplay, int width, int height, int cropX, int cropY, int cropWidth, int cropHeight)
{
    if (width < 1 || height < 1)
        throw Exception ("Invalid texture size.");

    if (cropX < 0 || cropY < 0 || cropWidth < 1 || cropHeight < 1)
        throw Exception ("Invalid crop.");


    this->width = width;
    this->height = height;



	int dmafd = CreateBuffer(drmfd, dpy, width, height, 32);

	// Map the buffer to userspace
	frame = mmap(NULL, width * height, PROT_READ | PROT_WRITE, MAP_SHARED, dmafd, 0);
	if (frame == MAP_FAILED)
	{
		throw Exception("mmap failed.");
	}


	int dmafd2 = CreateBuffer(drmfd, dpy, width, height / 2, 32);

	// Map the buffer to userspace (TODO: size to UV)
	frame2 = mmap(NULL, width * height / 2, PROT_READ | PROT_WRITE, MAP_SHARED, dmafd2, 0);
	if (frame2 == MAP_FAILED)
	{
		throw Exception("mmap 2 failed.");
	}


	// EGL_EXT_image_dma_buf_import
	EGLint img_attrs[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12	, //DRM_FORMAT_NV12,
		EGL_DMA_BUF_PLANE0_FD_EXT, dmafd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, width,
		EGL_DMA_BUF_PLANE1_FD_EXT, dmafd2,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE1_PITCH_EXT, width,
		EGL_YUV_COLOR_SPACE_HINT_EXT, EGL_ITU_REC601_EXT,
		EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_NARROW_RANGE_EXT,
		EGL_NONE
	};

	EGLImageKHR image = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, img_attrs);
	Egl::CheckError();

	fprintf(stderr, "EGLImageKHR = %p\n", image);


	// Texture
	glActiveTexture(GL_TEXTURE0);
	OpenGL::CheckError();

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, yTexture);
	OpenGL::CheckError();

	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	OpenGL::CheckError();

	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	OpenGL::CheckError();

	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
	OpenGL::CheckError();





#if 0
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
#endif

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

GLuint Scene::GetTexutreForDmabuf(int dmafd, int dmafd2)
{
	// TODO: account for different dmabuf pairs

	GLuint result;
	
	auto iter = dmabufMap.find(dmafd);
	if (iter == dmabufMap.end())
	{
		// Create texture
		GLuint texture;
		glGenTextures(1, &texture);
		OpenGL::CheckError();

		dmabufMap[dmafd] = texture;
		result = texture;


		// EGL_EXT_image_dma_buf_import
		EGLint img_attrs[] = {
			EGL_WIDTH, width,
			EGL_HEIGHT, height,
			EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12	, //DRM_FORMAT_NV12,
			EGL_DMA_BUF_PLANE0_FD_EXT, dmafd,
			EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
			EGL_DMA_BUF_PLANE0_PITCH_EXT, width,
			EGL_DMA_BUF_PLANE1_FD_EXT, dmafd2,
			EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
			EGL_DMA_BUF_PLANE1_PITCH_EXT, width,
			EGL_YUV_COLOR_SPACE_HINT_EXT, EGL_ITU_REC601_EXT,
			EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_NARROW_RANGE_EXT,
			EGL_NONE
		};

		EGLImageKHR image = eglCreateImageKHR(x11Window->EglDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, img_attrs);
		Egl::CheckError();

		fprintf(stderr, "EGLImageKHR = %p, width=%d, height=%d\n", image, width, height);


		// Texture
		glActiveTexture(GL_TEXTURE0);
		OpenGL::CheckError();

		glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
		OpenGL::CheckError();

		glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		OpenGL::CheckError();

		glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		OpenGL::CheckError();

		glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
		OpenGL::CheckError();
	}
	else
	{
		result = iter->second;
	}

	return result;
}

void Scene::Draw(int yData, int vuData)
{
	GLuint y = GetTexutreForDmabuf(yData, vuData);


	glActiveTexture(GL_TEXTURE0);
	OpenGL::CheckError();
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, y);
	OpenGL::CheckError();


	// Set texture crop
	float scaleX = (float)cropWidth / (float)width;
	float scaleY = (float)cropHeight / (float)height;

	glUniform2f(textureScaleUniform, scaleX, scaleY);
	OpenGL::CheckError();


	float offsetX = (float)cropX / (float)width;
	float offsetY = (float)cropY / (float)height;

	glUniform2f(textureOffsetUniform, offsetX, offsetY);
	OpenGL::CheckError();

	//glActiveTexture(GL_TEXTURE1);
	//OpenGL::CheckError();
	//glBindTexture(GL_TEXTURE_EXTERNAL_OES, uv);
	//OpenGL::CheckError();

#if 0
    if (yData == nullptr || vuData == nullptr)
        throw Exception("Invalid arguments.");

    if (width < 1 || height < 1)
        throw Exception ("Invalid texture state.");


	memcpy(frame, yData, width * height);
	memcpy(frame2, vuData, width * height / 2);
	//memcpy((uint8_t*)frame + (width * height), vuData, width * height / 2);

	glActiveTexture(GL_TEXTURE0);
	OpenGL::CheckError();

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, yTexture);
	OpenGL::CheckError();
#endif

#if 0
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
#endif



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
