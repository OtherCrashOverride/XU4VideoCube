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



int main()
{
    printf("Odroid XU4 Video Cube\n");
    printf("  Spacebar - toggle between quad and cube rendering\n");
    printf("  Escape   - exit\n");
    printf("\n");

    X11Window window(1280, 720, "Odroid XU4 Video Cube");


    Scene scene;
    scene.Load();


    //[    4.138152] [c7] s5p-mfc 11000000.mfc: decoder registered as /dev/video6
    const char* decoderName = "/dev/video6";


    // O_NONBLOCK prevents deque operations from blocking if no buffers are ready
    int mfc_fd = open(decoderName, O_RDWR | O_NONBLOCK, 0);
    if (mfc_fd < 0)
    {
        throw Exception("Failed to open MFC");
    }


    // Check device capabilities
    v4l2_capability cap = {0};

    int ret = ioctl(mfc_fd, VIDIOC_QUERYCAP, &cap);
    if (ret != 0)
    {
        throw Exception("VIDIOC_QUERYCAP failed.");
    }

    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) == 0 ||
            (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) == 0 ||
            (cap.capabilities & V4L2_CAP_STREAMING) == 0)
    {
        throw Exception("Insufficient capabilities of MFC device.");
    }


    // Create input buffers
    /* This is the size of the buffer for the compressed stream.
     * It limits the maximum compressed frame size. */
    const int STREAM_BUFFER_SIZE = (1024 * 1024);
    /* The number of compress4ed stream buffers */
    const int STREAM_BUFFER_CNT = 8;


    // Codec input
    v4l2_format fmtIn = {0};
    fmtIn.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmtIn.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmtIn.fmt.pix_mp.plane_fmt[0].sizeimage = (uint)STREAM_BUFFER_SIZE;
    fmtIn.fmt.pix_mp.num_planes = 1;

    if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtIn) != 0)
        throw Exception("Input format not supported.");

    // Reqest input buffers
    std::vector<std::shared_ptr<MediaStreamCodecData>> inBuffers =
                CodecData::RequestBuffers<MediaStreamCodecData>(mfc_fd,
                        V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                        V4L2_MEMORY_MMAP,
                        STREAM_BUFFER_CNT,
                        false);


    // Codec output
    v4l2_format fmtOut = {0};
    fmtOut.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmtOut.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
    fmtOut.fmt.pix_mp.num_planes = 2;

    if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtOut) != 0)
        throw  Exception("Output format not supported.");


    // Disable slice mode
    v4l2_control sliceCtrl= {0};
    sliceCtrl.id = V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE;
    sliceCtrl.value = 1;

    ret = ioctl(mfc_fd, VIDIOC_S_CTRL, &sliceCtrl);
    if (ret != 0)
    {
        throw Exception("VIDIOC_S_CTRL failed.");
    }


    // Read the file in chucks
    int fd = open("test.h264", O_RDONLY);
    if (fd < 0)
    {
        throw Exception ("test.h264 could not be opened.");
    }

    std::vector<unsigned char> bytes;
    bool isStreaming = false;
    int currentBuffer = 0;

    std::vector<std::shared_ptr<NV12CodecData>> decodeBuffers;

    int start = 0xffffffff;

    std::queue<int> freeBuffers;

    for(size_t i = 0; i < inBuffers.size(); ++i)
    {
        std::shared_ptr<MediaStreamCodecData> item = inBuffers[i];
        freeBuffers.push(item->Index);
    }


    const int bufferCount = 1024;
    unsigned char buffer[bufferCount];
    int offset = bufferCount;

    bool isRunning = true;
    while (isRunning)
    {
        // Reading a single byte at a time is very slow.
        // Fill a buffer and read from that instead.
        if (offset >= bufferCount)
        {
            int bytesRead = read(fd, &buffer, bufferCount);
            if (bytesRead < 1)
            {

                lseek(fd, 0, SEEK_SET);
                if (read(fd, &buffer + bytesRead, bufferCount - bytesRead) < 1)
                    throw Exception("Problem reading file.");
            }

            offset = 0;
        }

        unsigned char read = buffer[offset];
        ++offset;

        bytes.push_back(read);

        // keep track of last four (4) bytes for start code
        start = start << 8;
        start |= read;

        if (bytes.size() > 4 &&
                start == 0x00000001)
        {
            // Start code detected
            //printf("Start Code.\n");

            bool queueAccepted = false;
            while (!queueAccepted)
            {
                if (freeBuffers.size() > 0)
                {
                    // Copy the data to the buffer
                    currentBuffer = freeBuffers.front();
                    freeBuffers.pop();

                    memcpy(inBuffers[currentBuffer]->GetBuffer()->Address, &bytes[0], bytes.size() - 4);


                    // Enque the data to the codec
                    v4l2_buffer qbuf = {0};
                    v4l2_plane qplanes[1];

                    // bytes.size() - 4 is the data without the next startcode
                    qplanes[0].bytesused = (uint)(bytes.size() - 4);

                    qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                    qbuf.memory = V4L2_MEMORY_MMAP;
                    qbuf.index = (uint)currentBuffer;
                    qbuf.m.planes = qplanes;
                    qbuf.length = 1;

                    ret = ioctl(mfc_fd, VIDIOC_QBUF, &qbuf);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_QBUF failed.");
                    }

                    queueAccepted = true;
                }


                if (isStreaming)
                {
                    // Note: Must have valid planes because its written to by codec
                    v4l2_plane dqplanes[2];

                    v4l2_buffer dqbuf = {0};

                    // Deque decoded
                    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                    dqbuf.memory = V4L2_MEMORY_MMAP;
                    dqbuf.m.planes = dqplanes;
                    dqbuf.length = 2;

                    ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
                    if (ret != 0)
                    {
                        //printf("Waiting on V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer.\n");
                    }
                    else
                    {
                        //printf("V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer dequeued.\n");

                        scene.Draw(decodeBuffers[(int)dqbuf.index]->GetY()->Address,
                                   decodeBuffers[(int)dqbuf.index]->GetVU()->Address);

                        window.SwapBuffers();


                        // Re-queue buffer
                        ret = ioctl(mfc_fd, VIDIOC_QBUF, &dqbuf);
                        if (ret != 0)
                        {
                            throw Exception("VIDIOC_QBUF failed.");
                        }
                    }


                    // Deque input
                    v4l2_plane dqplanes2[1];

                    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                    dqbuf.memory = V4L2_MEMORY_MMAP;
                    dqbuf.m.planes = dqplanes2;
                    dqbuf.length = 1;

                    ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
                    if (ret != 0)
                    {
                        //printf("Waiting on V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE buffer.\n");
                    }
                    else
                    {
                        freeBuffers.push((int)dqbuf.index);

                        //printf("V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE buffer dequeued.\n");
                    }
                }
                else
                {
                    // Must have at least one buffer queued before streaming can start
                    int val = (int)V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

                    ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_STREAMON failed.");
                    }


                    // After header is parsed, capture queue can be setup

                    // Get the image type and dimensions
                    v4l2_format captureFormat = {0};
                    captureFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

                    ret = ioctl(mfc_fd, VIDIOC_G_FMT, &captureFormat);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_G_FMT failed.");
                    }

                    int captureWidth = (int)captureFormat.fmt.pix_mp.width;
                    int captureHeigh = (int)captureFormat.fmt.pix_mp.height;


                    // Get the number of buffers required
                    v4l2_control ctrl = {0};
                    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

                    ret = ioctl(mfc_fd, VIDIOC_G_CTRL, &ctrl);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_G_CTRL failed.");
                    }

                    // request the buffers
                    decodeBuffers = CodecData::RequestBuffers<NV12CodecData>(mfc_fd,
                                    V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                                    V4L2_MEMORY_MMAP,
                                    ctrl.value,
                                    true);


                    // Get the crop information.  Codecs work on multiples of a number,
                    // so the image decoded may be larger and need cropping.
                    v4l2_crop crop = {0};
                    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

                    ret = ioctl(mfc_fd, VIDIOC_G_CROP, &crop);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_G_CROP failed.");
                    }


                    // Creat the rendering textures
                    scene.CreateTextures(captureWidth, captureHeigh,
                                         crop.c.left, crop.c.top,
                                         crop.c.width, crop.c.height);


                    // Start streaming the capture
                    val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

                    ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
                    if (ret != 0)
                    {
                        throw Exception("VIDIOC_STREAMON failed.");
                    }

                    isStreaming = true;
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
                }
                while (action != Action::Nothing);

                //Yield
                std::this_thread::yield();
        }

        //printf("Buffer sent to codec.\n");

        // Clear the data
        bytes.clear();

            // Add the start code back
            bytes.push_back(0x00);
            bytes.push_back(0x00);
            bytes.push_back(0x00);
            bytes.push_back(0x01);
        }
    }

    close(fd);
    close(mfc_fd);

    return 0;
}
