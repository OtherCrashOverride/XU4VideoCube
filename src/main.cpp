#include "X11Window.h"
#include "Matrix4.h"
#include "NV12Shader.h"
#include "OpenGL.h"
#include "CodecData.h"
#include "Scene.h"
#include <GLES2/gl2.h>
#include <queue>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <signal.h>

#include <chrono>

#include <xf86drm.h>
#include <drm/drm_fourcc.h>
#include <sys/mman.h>

// libdri2 is not aware of C++
extern "C"
{
#include <X11/extensions/dri2.h>
}

#include "Mfc.h"


bool isRunning;


// Signal handler for Ctrl-C
void SignalHandler(int s)
{
	isRunning = false;
}

int OpenDRM(Display* dpy)
{
	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		throw Exception("DRM device open failed.");
	}

	uint64_t hasDumb;
	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &hasDumb) < 0 ||
		!hasDumb)
	{
		throw Exception("DRM device does not support dumb buffers");
	}


	// Obtain DRM authorization
	drm_magic_t magic;
	if (drmGetMagic(fd, &magic))
	{
		throw Exception("drmGetMagic failed");
	}

	Window root = RootWindow(dpy, DefaultScreen(dpy));
	if (!DRI2Authenticate(dpy, root, magic))
	{
		throw Exception("DRI2Authenticate failed");
	}


	return fd;
}


int main()
{
	printf("Odroid XU4 Video Cube\n");
	printf("  Spacebar - toggle between quad and cube rendering\n");
	printf("  Escape   - exit\n");
	printf("\n");

	X11Window window(1280, 720, "Odroid XU4 Video Cube");


	// Authenticate with DRM
	int drmfd = OpenDRM(window.X11Display());


	// --

	Scene scene(&window);
	scene.Load();

	Mfc mfc;


	// Read the file in chucks
	int fd = open("test.h264", O_RDONLY);
	if (fd < 0)
	{
		throw Exception("test.h264 could not be opened.");
	}

	std::vector<unsigned char> bytes;
	bool isStreaming = false;
	int currentBuffer = 0;
	int start = 0xffffffff;

	const int bufferCount = 1024 * 4;
	unsigned char buffer[bufferCount];
	int offset = bufferCount;

	isRunning = true;

	// Trap signal to clean up
	signal(SIGINT, SignalHandler);


	auto startTime = std::chrono::high_resolution_clock::now();
	int frames = 0;
	double totalElapsed = 0;


	// Catch exceptions to cleanup MFC.
	try
	{
		while (isRunning)
		{
			// Reading a single byte at a time is very slow.
			// Fill a buffer and read from that instead.
			if (offset >= bufferCount)
			{
				int bytesRead = read(fd, buffer, bufferCount);
				if (bytesRead < 1)
				{

					lseek(fd, 0, SEEK_SET);
					if (read(fd, buffer + bytesRead, bufferCount - bytesRead) < 1)
						throw Exception("Problem reading file.");
				}

				offset = 0;
			}

			while (offset < bufferCount)
			{
				unsigned char read = buffer[offset];
				++offset;

				bytes.push_back(read);

				// keep track of last four (4) bytes for start code
				start = start << 8;
				start |= read;

				if (start == 0x00000001)
					break;
			}

			if (bytes.size() > 4 &&
				start == 0x00000001)
			{
				// Start code detected
				//printf("Start Code.\n");
				
					
				mfc.Enqueue(bytes);
				//printf("mfc.Enqueue\n");

				if (!isStreaming)
				{
					int captureWidth;
					int captureHeigh;
					v4l2_crop crop;
					mfc.StreamOn(captureWidth, captureHeigh, crop);

					// Creat the rendering textures
					scene.SetTextureProperties(captureWidth, captureHeigh,
							crop.c.left, crop.c.top,
							crop.c.width, crop.c.height);

					isStreaming = true;
				}
					

				if (isStreaming)
				{
					if (mfc.Dequeue(scene))
					{
						window.SwapBuffers();

						++frames;

						auto endTime = std::chrono::high_resolution_clock::now();
						auto elapsed = std::chrono::duration<double>(endTime - startTime);
						startTime = endTime;
						totalElapsed += elapsed.count();

						if (totalElapsed >= 1.0)
						{
							double fps = frames / totalElapsed;
							printf("FPS: %f\n", fps);

							frames = 0;
							totalElapsed = 0;
						}
					}
				}
				else
				{

				}

				// Process any events
				Action action;
				do
				{
					action = window.ProcessMessages();

					switch (action)
					{
					case Action::ChangeScene:
						if (scene.GetSceneMode() == SceneMode::Cube)
						{
							scene.SetSceneMode(SceneMode::Quad);
						}
						else
						{
							scene.SetSceneMode(SceneMode::Cube);
						}
						break;

					case Action::Quit:
						isRunning = false;
						break;

					default:
						break;
					}
				} while (action != Action::Nothing);

				//Yield
				//std::this_thread::yield();

				// Clear the data
				bytes.clear();

				// Add the start code back
				bytes.push_back(0x00);
				bytes.push_back(0x00);
				bytes.push_back(0x00);
				bytes.push_back(0x01);
			}
		}

	}
	catch (...)
	{
		printf("Caught exception. Exiting.\n");
	}


	close(fd);
	close(drmfd);

	return 0;
}
