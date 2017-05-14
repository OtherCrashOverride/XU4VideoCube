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

#include <thread>
#include <mutex>
#include <chrono>


bool isRunning;

class Mfc
{
	//[    4.138152] [c7] s5p-mfc 11000000.mfc: decoder registered as /dev/video6
	//[    2.236569] s5p-mfc 11000000.codec:: decoder registered as /dev/video10
	const char* decoderName = "/dev/video10";

	int mfc_fd = -1;
	std::vector<std::shared_ptr<MediaStreamCodecData>> inBuffers;
	std::vector<std::shared_ptr<NV12CodecData>> outBuffers;

	std::thread thread;
	std::mutex mutex;

	bool isRunning = true;
	//std::shared_ptr<NV12CodecData> available;
	std::queue<std::shared_ptr<NV12CodecData>> availableOutBuffers;


	void WorkThread()
	{
		// Get the number of buffers required
		v4l2_control ctrl = { 0 };
		ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

		int ret = ioctl(mfc_fd, VIDIOC_G_CTRL, &ctrl);
		if (ret != 0)
		{
			throw Exception("VIDIOC_G_CTRL failed.");
		}

		// request the buffers
		outBuffers = CodecData::RequestBuffers<NV12CodecData>(mfc_fd,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			ctrl.value,
			true);


		// Start streaming the capture
		int val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
		if (ret != 0)
		{
			throw Exception("VIDIOC_STREAMON failed.");
		}

		while (isRunning)
		{
			// Note: Must have valid planes because its written to by codec
			v4l2_plane dqplanes[4] = { 0 };
			v4l2_buffer dqbuf = { 0 };

			// Deque decoded
			dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			dqbuf.memory = V4L2_MEMORY_MMAP;
			dqbuf.m.planes = dqplanes;
			dqbuf.length = 2;

			int ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
			if (ret < 0)
			{
				throw Exception("Dequeue: VIDIOC_DQBUF failed.");
			}

			//printf("V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer dequeued.\n");

			//scene.Draw(decodeBuffers[(int)dqbuf.index]->GetY()->Address,
			//	decodeBuffers[(int)dqbuf.index]->GetVU()->Address);

			mutex.lock();
			//available = outBuffers[(int)dqbuf.index];
			availableOutBuffers.push(outBuffers[(int)dqbuf.index]);

			mutex.unlock();

			//window.SwapBuffers();


			//// Re-queue buffer
			//ret = ioctl(mfc_fd, VIDIOC_QBUF, &dqbuf);
			//if (ret != 0)
			//{
			//	throw Exception("Dequeue: VIDIOC_QBUF failed.");
			//}
		}
	}

public:
	std::queue<int> freeBuffers;


	Mfc()
	{
		// O_NONBLOCK prevents deque operations from blocking if no buffers are ready
		mfc_fd = open(decoderName, O_RDWR, 0);
		if (mfc_fd < 0)
		{
			throw Exception("Failed to open MFC");
		}


		// Check device capabilities
		v4l2_capability cap = { 0 };

		int ret = ioctl(mfc_fd, VIDIOC_QUERYCAP, &cap);
		if (ret != 0)
		{
			throw Exception("VIDIOC_QUERYCAP failed.");
		}

		if ((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) == 0 ||
			(cap.capabilities & V4L2_CAP_STREAMING) == 0)
		{
			printf("V4L2_CAP_VIDEO_M2M_MPLANE=%d\n", (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) != 0);
			printf("V4L2_CAP_STREAMING=%d\n", (cap.capabilities & V4L2_CAP_STREAMING) != 0);

			throw Exception("Insufficient capabilities of MFC device.");
		}


		// Create input buffers
		/* This is the size of the buffer for the compressed stream.
		* It limits the maximum compressed frame size. */
		const int STREAM_BUFFER_SIZE = (1024 * 1024);
		/* The number of compressed stream buffers */
		const int STREAM_BUFFER_CNT = 8;


		// Codec input
		v4l2_format fmtIn = { 0 };
		fmtIn.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		fmtIn.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
		fmtIn.fmt.pix_mp.plane_fmt[0].sizeimage = (uint)STREAM_BUFFER_SIZE;
		fmtIn.fmt.pix_mp.num_planes = 1;

		if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtIn) != 0)
			throw Exception("Input format not supported.");

		// Reqest input buffers
		printf("Requesting input buffers.\n");
		inBuffers = CodecData::RequestBuffers<MediaStreamCodecData>(mfc_fd,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			STREAM_BUFFER_CNT,
			false);
		printf("Requesting input buffers = OK.\n");

		for (size_t i = 0; i < inBuffers.size(); ++i)
		{
			std::shared_ptr<MediaStreamCodecData> item = inBuffers[i];
			freeBuffers.push(item->Index);
		}


		// Codec output
		v4l2_format fmtOut = { 0 };
		fmtOut.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmtOut.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
		fmtOut.fmt.pix_mp.num_planes = 2;

		if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtOut) != 0)
			throw  Exception("Output format not supported.");


		// Disable slice mode
		v4l2_control sliceCtrl = { 0 };
		sliceCtrl.id = V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE;
		sliceCtrl.value = 1;

		ret = ioctl(mfc_fd, VIDIOC_S_CTRL, &sliceCtrl);
		if (ret != 0)
		{
			throw Exception("VIDIOC_S_CTRL failed.");
		}
	}

	~Mfc()
	{
		// Stop streaming the capture
		//if (isStreaming)
		//{
		//	int val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		//	ret = ioctl(mfc_fd, VIDIOC_STREAMOFF, &val);
		//	if (ret != 0)
		//	{
		//		throw Exception("VIDIOC_STREAMOFF failed.");
		//	}
		//}
		isRunning = false;

		StreamOff();

		thread.join();

		close(mfc_fd);
	}

	void Enqueue(std::vector<unsigned char>& bytes)
	{
		//printf("Mfc::Enqueue: bytes.size()=%d\n", bytes.size());

		// Deque any available buffers
		while (true)
		{
			v4l2_plane dqplanes2[4];

			v4l2_buffer dqbuf = { 0 };
			dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			dqbuf.memory = V4L2_MEMORY_MMAP;
			dqbuf.m.planes = dqplanes2;
			dqbuf.length = 1;

			int ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
			if (ret < 0)
			{
				//throw Exception("Enqueue: VIDIOC_DQBUF failed.");
				break;
			}

			//printf("Mfc::Enqueue: dequeues buffer %d\n", dqbuf.index);
			freeBuffers.push((int)dqbuf.index);

			break;
		}

		if (freeBuffers.size() < 1)
			throw Exception("Enqueue: No free buffers.");


		// Copy the data to the buffer
		int currentBuffer = freeBuffers.front();
		freeBuffers.pop();

		//printf("Mfc::Enqueue: using buffer %d\n", currentBuffer);
		memcpy(inBuffers[currentBuffer]->GetBuffer()->Address, &bytes[0], bytes.size() - 4);


		// Enque the data to the codec
		v4l2_buffer qbuf = { 0 };
		v4l2_plane qplanes[4] = { 0 };

		// bytes.size() - 4 is the data without the next startcode
		qplanes[0].bytesused = (uint)(bytes.size() - 4);

		qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		qbuf.memory = V4L2_MEMORY_MMAP;
		qbuf.index = (uint)currentBuffer;
		qbuf.m.planes = qplanes;
		qbuf.length = 1;

		int ret = ioctl(mfc_fd, VIDIOC_QBUF, &qbuf);
		if (ret != 0)
		{
			printf("Enqueue: VIDIOC_QBUF failed (%d)\n", errno);
			throw Exception("Enqueue: VIDIOC_QBUF failed");
		}
	}

	bool Dequeue(Scene& scene)
	{
		//// Note: Must have valid planes because its written to by codec
		//v4l2_plane dqplanes[4] = { 0 };
		//v4l2_buffer dqbuf = { 0 };

		//// Deque decoded
		//dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		//dqbuf.memory = V4L2_MEMORY_MMAP;
		//dqbuf.m.planes = dqplanes;
		//dqbuf.length = 2;

		//int ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
		//if (ret < 0)
		//{
		//	throw Exception("Dequeue: VIDIOC_DQBUF failed.");
		//}

		////printf("V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer dequeued.\n");
		bool result = false;

		mutex.lock();

		if (availableOutBuffers.size() > 0)
		{
			result = true;

			std::shared_ptr<NV12CodecData> available = availableOutBuffers.front();
			availableOutBuffers.pop();

			scene.Draw(available->GetY()->Address, available->GetVU()->Address);

			// Re-queue buffer
			v4l2_plane dqplanes[4] = { 0 };
			v4l2_buffer dqbuf = { 0 };

			// Deque decoded
			dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			dqbuf.memory = V4L2_MEMORY_MMAP;
			dqbuf.m.planes = dqplanes;
			dqbuf.length = 2;
			dqbuf.index = available->Index;

			int ret = ioctl(mfc_fd, VIDIOC_QBUF, &dqbuf);
			if (ret != 0)
			{
				throw Exception("Dequeue: VIDIOC_QBUF failed.");
			}			
		}

		mutex.unlock();

		////window.SwapBuffers();


		//// Re-queue buffer
		//ret = ioctl(mfc_fd, VIDIOC_QBUF, &dqbuf);
		//if (ret != 0)
		//{
		//	throw Exception("Dequeue: VIDIOC_QBUF failed.");
		//}		

		return result;
	}

	void StreamOn(int& captureWidth, int& captureHeigh, v4l2_crop& crop)
	{
		// Must have at least one buffer queued before streaming can start
		int val = (int)V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

		int ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
		if (ret != 0)
		{
			throw Exception("VIDIOC_STREAMON failed.");
		}


		// After header is parsed, capture queue can be setup

		// Get the image type and dimensions
		v4l2_format captureFormat = { 0 };
		captureFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		ret = ioctl(mfc_fd, VIDIOC_G_FMT, &captureFormat);
		if (ret != 0)
		{
			throw Exception("VIDIOC_G_FMT failed.");
		}

		captureWidth = (int)captureFormat.fmt.pix_mp.width;
		captureHeigh = (int)captureFormat.fmt.pix_mp.height;


		// Get the crop information.  Codecs work on multiples of a number,
		// so the image decoded may be larger and need cropping.
		crop = { 0 };
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		ret = ioctl(mfc_fd, VIDIOC_G_CROP, &crop);
		if (ret != 0)
		{
			throw Exception("VIDIOC_G_CROP failed.");
		}


		isRunning = true;
		thread = std::thread(&Mfc::WorkThread, this);
	}

	void StreamOff()
	{
		int val;
		int ret;

		val = (int)V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		ret = ioctl(mfc_fd, VIDIOC_STREAMOFF, &val);
		if (ret != 0)
		{
			throw Exception("VIDIOC_STREAMON failed.");
		}


		val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ret = ioctl(mfc_fd, VIDIOC_STREAMOFF, &val);
		if (ret != 0)
		{
			throw Exception("VIDIOC_STREAMON failed.");
		}
	}
};



// Signal handler for Ctrl-C
void SignalHandler(int s)
{
	isRunning = false;
}


int main()
{
	printf("Odroid XU4 Video Cube\n");
	printf("  Spacebar - toggle between quad and cube rendering\n");
	printf("  Escape   - exit\n");
	printf("\n");

	X11Window window(1280, 720, "Odroid XU4 Video Cube");


	Scene scene;
	scene.Load();

#if 0
	//   //[    4.138152] [c7] s5p-mfc 11000000.mfc: decoder registered as /dev/video6
	   ////[    2.236569] s5p-mfc 11000000.codec:: decoder registered as /dev/video10
	//   const char* decoderName = "/dev/video10";


	//   // O_NONBLOCK prevents deque operations from blocking if no buffers are ready
	//   int mfc_fd = open(decoderName, O_RDWR | O_NONBLOCK, 0);
	//   if (mfc_fd < 0)
	//   {
	//       throw Exception("Failed to open MFC");
	//   }


	//   // Check device capabilities
	//   v4l2_capability cap = {0};

	//   int ret = ioctl(mfc_fd, VIDIOC_QUERYCAP, &cap);
	//   if (ret != 0)
	//   {
	//       throw Exception("VIDIOC_QUERYCAP failed.");
	//   }

	//   if ((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) == 0 ||
	//       (cap.capabilities & V4L2_CAP_STREAMING) == 0)
	//   {
	   //	printf("V4L2_CAP_VIDEO_M2M_MPLANE=%d\n", (cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE) != 0);
	   //	printf("V4L2_CAP_STREAMING=%d\n", (cap.capabilities & V4L2_CAP_STREAMING) != 0);
	   //
	//       throw Exception("Insufficient capabilities of MFC device.");
	//   }


	//   // Create input buffers
	//   /* This is the size of the buffer for the compressed stream.
	//    * It limits the maximum compressed frame size. */
	//   const int STREAM_BUFFER_SIZE = (1024 * 1024);
	//   /* The number of compressed stream buffers */
	//   const int STREAM_BUFFER_CNT = 8;


	//   // Codec input
	//   v4l2_format fmtIn = {0};
	//   fmtIn.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	//   fmtIn.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
	//   fmtIn.fmt.pix_mp.plane_fmt[0].sizeimage = (uint)STREAM_BUFFER_SIZE;
	//   fmtIn.fmt.pix_mp.num_planes = 1;

	//   if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtIn) != 0)
	//       throw Exception("Input format not supported.");

	//   // Reqest input buffers
	   //printf("Requesting input buffers.\n");
	//   std::vector<std::shared_ptr<MediaStreamCodecData>> inBuffers =
	//               CodecData::RequestBuffers<MediaStreamCodecData>(mfc_fd,
	//                       V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	//                       STREAM_BUFFER_CNT,
	//                       false);
	   //printf("Requesting input buffers = OK.\n");

	//   // Codec output
	//   v4l2_format fmtOut = {0};
	//   fmtOut.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	//   fmtOut.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
	//   fmtOut.fmt.pix_mp.num_planes = 2;

	//   if (ioctl(mfc_fd, VIDIOC_S_FMT, &fmtOut) != 0)
	//       throw  Exception("Output format not supported.");


	//   // Disable slice mode
	//   v4l2_control sliceCtrl= {0};
	//   sliceCtrl.id = V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE;
	//   sliceCtrl.value = 1;

	//   ret = ioctl(mfc_fd, VIDIOC_S_CTRL, &sliceCtrl);
	//   if (ret != 0)
	//   {
	//       throw Exception("VIDIOC_S_CTRL failed.");
	//   }
#endif

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

	//std::vector<std::shared_ptr<NV12CodecData>> decodeBuffers;

	int start = 0xffffffff;

	//std::queue<int> freeBuffers;

	//for(size_t i = 0; i < inBuffers.size(); ++i)
	//{
	//    std::shared_ptr<MediaStreamCodecData> item = inBuffers[i];
	//    freeBuffers.push(item->Index);
	//}


	const int bufferCount = 1024 * 4;
	unsigned char buffer[bufferCount];
	int offset = bufferCount;

	isRunning = true;

	// Trap signal to clean up
	signal(SIGINT, SignalHandler);

	//std::array<uint32_t, 1024 * 1024> bufferData;
	//int bufferDataStart = 0;
	//int bufferDataLength = 0;

	auto startTime = std::chrono::high_resolution_clock::now();
	int frames = 0;
	double totalElapsed = 0;

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

			//bytes.resize(bufferCount);
			//memcpy(&bytes[0], buffer, bufferCount);
			//offset += bufferCount;

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

				//bool queueAccepted = false;
				//while (!queueAccepted)
				{
					if (mfc.freeBuffers.size() > 0)
					{
#if 0
						//               // Copy the data to the buffer
						//               currentBuffer = freeBuffers.front();
						//               freeBuffers.pop();

						//               memcpy(inBuffers[currentBuffer]->GetBuffer()->Address, &bytes[0], bytes.size() - 4);


						//               // Enque the data to the codec
						//               v4l2_buffer qbuf = {0};
									   //v4l2_plane qplanes[4] = { 0 }; //v4l2_plane qplanes[1];

						//               // bytes.size() - 4 is the data without the next startcode
						//               qplanes[0].bytesused = (uint)(bytes.size() - 4);

						//               qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
						//               qbuf.memory = V4L2_MEMORY_MMAP;
						//               qbuf.index = (uint)currentBuffer;
						//               qbuf.m.planes = qplanes;
						//               qbuf.length = 1;

						//               ret = ioctl(mfc_fd, VIDIOC_QBUF, &qbuf);
						//               if (ret != 0)
						//               {
									   //	perror("VIDIOC_QBUF failed");
						//                   throw Exception("VIDIOC_QBUF failed");
						//               }
#endif

						mfc.Enqueue(bytes);
						//printf("mfc.Enqueue\n");

						if (!isStreaming)
						{
							int captureWidth;
							int captureHeigh;
							v4l2_crop crop;
							mfc.StreamOn(captureWidth, captureHeigh, crop);

							// Creat the rendering textures
							scene.CreateTextures(captureWidth, captureHeigh,
								crop.c.left, crop.c.top,
								crop.c.width, crop.c.height);

							isStreaming = true;
						}

						//// Start streaming the capture
						//val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

						//ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
						//if (ret != 0)
						//{
						//    throw Exception("VIDIOC_STREAMON failed.");
						//}

						

						//queueAccepted = true;
					}


					if (isStreaming)
					{
#if 0
						//               // Note: Must have valid planes because its written to by codec
									   //v4l2_plane dqplanes[4] = { 0 };
						//               v4l2_buffer dqbuf = {0};

						//               // Deque decoded
						//               dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
						//               dqbuf.memory = V4L2_MEMORY_MMAP;
						//               dqbuf.m.planes = dqplanes;
						//               dqbuf.length = 2;

						//               ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
						//               if (ret != 0)
						//               {
						//                   //printf("Waiting on V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer.\n");
						//               }
						//               else
						//               {
						//                   //printf("V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE buffer dequeued.\n");
#endif

						//scene.Draw(decodeBuffers[(int)dqbuf.index]->GetY()->Address,
						//           decodeBuffers[(int)dqbuf.index]->GetVU()->Address);

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

#if 0
						//                   // Re-queue buffer
						//                   ret = ioctl(mfc_fd, VIDIOC_QBUF, &dqbuf);
						//                   if (ret != 0)
						//                   {
						//                       throw Exception("VIDIOC_QBUF failed.");
						//                   }
						//               }


						//               // Deque input
						//               v4l2_plane dqplanes2[4];

						//               dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
						//               dqbuf.memory = V4L2_MEMORY_MMAP;
						//               dqbuf.m.planes = dqplanes2;
						//               dqbuf.length = 1;

						//               ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
						//               if (ret != 0)
						//               {
						//                   //printf("Waiting on V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE buffer.\n");
						//               }
						//               else
						//               {
						//                   freeBuffers.push((int)dqbuf.index);

						//                   //printf("V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE buffer dequeued.\n");
						//               }
#endif
					}
					else
					{
#if 0
						//// Must have at least one buffer queued before streaming can start
						//int val = (int)V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

						//ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
						//if (ret != 0)
						//{
						//    throw Exception("VIDIOC_STREAMON failed.");
						//}

						//// After header is parsed, capture queue can be setup

						//// Get the image type and dimensions
						//v4l2_format captureFormat = {0};
						//captureFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

						//ret = ioctl(mfc_fd, VIDIOC_G_FMT, &captureFormat);
						//if (ret != 0)
						//{
						//    throw Exception("VIDIOC_G_FMT failed.");
						//}

						//int captureWidth = (int)captureFormat.fmt.pix_mp.width;
						//int captureHeigh = (int)captureFormat.fmt.pix_mp.height;


						//// Get the number of buffers required
						//v4l2_control ctrl = {0};
						//ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

						//ret = ioctl(mfc_fd, VIDIOC_G_CTRL, &ctrl);
						//if (ret != 0)
						//{
						//    throw Exception("VIDIOC_G_CTRL failed.");
						//}

						//// request the buffers
						//decodeBuffers = CodecData::RequestBuffers<NV12CodecData>(mfc_fd,
						//                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
						//                ctrl.value,
						//                true);


						//// Get the crop information.  Codecs work on multiples of a number,
						//// so the image decoded may be larger and need cropping.
						//v4l2_crop crop = {0};
						//crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

						//ret = ioctl(mfc_fd, VIDIOC_G_CROP, &crop);
						//if (ret != 0)
						//{
						//    throw Exception("VIDIOC_G_CROP failed.");
						//}
#endif

					//int captureWidth;
					//int captureHeigh;
					//v4l2_crop crop;
					//mfc.StreamOn(captureWidth, captureHeigh, crop);

	 //               // Creat the rendering textures
	 //               scene.CreateTextures(captureWidth, captureHeigh,
	 //                                    crop.c.left, crop.c.top,
	 //                                    crop.c.width, crop.c.height);


	 //               //// Start streaming the capture
	 //               //val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	 //               //ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
	 //               //if (ret != 0)
	 //               //{
	 //               //    throw Exception("VIDIOC_STREAMON failed.");
	 //               //}

	 //               isStreaming = true;
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

	}
	catch (const std::exception&)
	{
		printf("Caught exception. Exiting.\n");
	}

	//// Stop streaming the capture
	//if (isStreaming)
	//{
	//	int val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	//	ret = ioctl(mfc_fd, VIDIOC_STREAMOFF, &val);
	//	if (ret != 0)
	//	{
	//		throw Exception("VIDIOC_STREAMOFF failed.");
	//	}
	//}

	close(fd);
	//close(mfc_fd);

	return 0;
}
