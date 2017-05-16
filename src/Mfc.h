#pragma once

class Mfc
{
	//[    4.138152] [c7] s5p-mfc 11000000.mfc: decoder registered as /dev/video6
	//[    2.236569] s5p-mfc 11000000.codec:: decoder registered as /dev/video10
	const char* decoderName = "/dev/video10";

	int mfc_fd = -1;
	std::vector<std::shared_ptr<MediaStreamCodecData>> inBuffers;
	std::vector<std::shared_ptr<NV12CodecData>> outBuffers;
	std::queue<int> freeBuffers;


public:


	Mfc()
	{
		// O_NONBLOCK prevents deque operations from blocking if no buffers are ready
		mfc_fd = open(decoderName, O_RDWR | O_NONBLOCK, 0);
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
		StreamOff();

		close(mfc_fd);
	}

	void Enqueue(std::vector<unsigned char>& bytes)
	{
		//printf("Mfc::Enqueue: bytes.size()=%d\n", bytes.size());

		// Deque any available buffers
		while (freeBuffers.size() < 1)
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
				//if (ret != -EAGAIN)
				//	break;

				std::this_thread::yield();
			}
			else
			{
				//printf("Mfc::Enqueue: dequeues buffer %d\n", dqbuf.index);
				freeBuffers.push((int)dqbuf.index);
			}
			//break;
		}

		//if (freeBuffers.size() < 1)
		//	throw Exception("Enqueue: No free buffers.");


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
		int val;
		int ret;
		bool result = false;


		// Note: Must have valid planes because its written to by codec
		v4l2_plane dqplanes[4] = { 0 };
		v4l2_buffer dqbuf = { 0 };

		// Deque decoded
		dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		dqbuf.memory = V4L2_MEMORY_MMAP;
		dqbuf.m.planes = dqplanes;
		dqbuf.length = 2;

		ret = ioctl(mfc_fd, VIDIOC_DQBUF, &dqbuf);
		if (ret == 0)
		{
			result = true;

			std::shared_ptr<NV12CodecData> available = outBuffers[(int)dqbuf.index];

			scene.Draw(available->GetY()->Dmabuf, available->GetVU()->Dmabuf);

			// Re-queue buffer
			v4l2_plane dqplanes[4] = { 0 };
			v4l2_buffer qbuf = { 0 };
			qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			qbuf.memory = V4L2_MEMORY_MMAP;
			qbuf.m.planes = dqplanes;
			qbuf.length = 4;
			qbuf.index = available->Index;

			int ret = ioctl(mfc_fd, VIDIOC_QUERYBUF, &qbuf);
			if (ret != 0)
			{
				throw Exception("VIDIOC_QUERYBUF failed.");
			}

			// Requeue buffer
			ret = ioctl(mfc_fd, VIDIOC_QBUF, &qbuf);
			if (ret != 0)
			{
				//printf("Dequeue: VIDIOC_QBUF failed. (%d)", ret);
				throw Exception("Dequeue: VIDIOC_QBUF failed.");
			}
		}

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

		//std::this_thread::sleep_for(std::chrono::seconds(1));

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


		// Get the number of buffers required
		v4l2_control ctrl = { 0 };
		ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

		ret = ioctl(mfc_fd, VIDIOC_G_CTRL, &ctrl);
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
		val = (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

		ret = ioctl(mfc_fd, VIDIOC_STREAMON, &val);
		if (ret != 0)
		{
			throw Exception("VIDIOC_STREAMON failed.");
		}
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
