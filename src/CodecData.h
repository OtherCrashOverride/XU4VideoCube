#pragma once

#include "Exception.h"

#include <memory>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>

class Buffer
{
public:
    size_t Offset = 0;
    void* Address = 0;
    size_t Length = 0;
	int Dmabuf = 0;
};

class CodecData
{
public:
    int Index;
    int  planesCount;

protected:
    std::vector<std::shared_ptr<Buffer>> planes;


    CodecData(int  planesCount)
        : planesCount(planesCount)
    {
        //planes.reserve(planesCount);

        for (int i = 0; i < planesCount; ++i)
        {
            planes.push_back(std::shared_ptr<Buffer>(new Buffer()));
        }
    }


public:
    virtual ~CodecData()
    {
    }


    template<typename T>
    static std::vector<std::shared_ptr<T>> RequestBuffers(int fd, v4l2_buf_type type, int count, bool enqueue) //where T: CodecData, new()
    {
		// Note: MFC (4.9.y) only supports V4L2_MEMORY_MMAP

        // Reqest input buffers
        v4l2_requestbuffers reqbuf = {0};
        reqbuf.count = (uint)count;
        reqbuf.type = (uint)type;
        reqbuf.memory = V4L2_MEMORY_MMAP;	

        if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) != 0)
            throw Exception("VIDIOC_REQBUFS failed.");


        // Get the buffer details
        std::vector<std::shared_ptr<T>> inBuffers;
        //inBuffers.reserve(reqbuf.count);    // the actual count may differ from requested count


        for (int n = 0; n < (int)reqbuf.count; ++n)
        {
            T* temp = new T();

            std::shared_ptr<T> codecData(temp);

            v4l2_plane planes[codecData->planesCount] = {0};


            v4l2_buffer buf = {0};
            buf.type = (uint)type;
            buf.memory = reqbuf.memory;
            buf.index = (uint)n;
            buf.m.planes = planes;
            buf.length = (uint)codecData->planesCount;

            int ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
            if (ret != 0)
            {
                throw Exception("VIDIOC_QUERYBUF failed.");
            }

            //inBuffers[n] = codecData;
            inBuffers.push_back(codecData);

            for (int i = 0; i < codecData->planesCount; ++i)
            {
				v4l2_exportbuffer expbuf = { 0 };
				expbuf.type = buf.type;
				expbuf.index = buf.index;
				expbuf.plane = i;
				expbuf.flags = O_RDWR;

				ret = ioctl(fd, VIDIOC_EXPBUF, &expbuf);
				if (ret < 0)
				{
					throw Exception("VIDIOC_EXPBUF failed.");
				}

				// ---
                codecData->Index = n;

                codecData->planes[i]->Offset = planes[i].m.mem_offset;
				
				codecData->planes[i]->Dmabuf = expbuf.fd;

                codecData->planes[i]->Address = mmap(nullptr, planes[i].length,
                                                     PROT_READ | PROT_WRITE,
                                                     MAP_SHARED,
                                                     fd,
                                                     (int)planes[i].m.mem_offset);

				//codecData->planes[i]->Address = mmap(NULL,
				//	planes[i].length,
				//	PROT_READ | PROT_WRITE,
				//	MAP_SHARED,
				//	expbuf.fd,
				//	0);
				if (codecData->planes[i]->Address == MAP_FAILED)
				{
					printf("mmap failed: dmabuf=%d\n", expbuf.fd);
					throw Exception("mmap failed.");
				}

                codecData->planes[i]->Length = planes[i].length;
            }

            if (enqueue)
            {
                // Enqueu buffer
                int ret = ioctl(fd, (uint)VIDIOC_QBUF, &buf);
                if (ret != 0)
                {
                    throw Exception("VIDIOC_QBUF failed.");
                }
            }
        }


        return inBuffers;
    }
};

class MediaStreamCodecData : public CodecData
{
public:
    std::shared_ptr<Buffer> GetBuffer() const
    {
        return planes[0];
    }

    MediaStreamCodecData()
        : CodecData(1)
    {
    }
};

class NV12CodecData : public CodecData
{
public:
    std::shared_ptr<Buffer> GetY() const
    {
        return planes[0];
    }

    std::shared_ptr<Buffer> GetVU() const
    {
        return planes[1];
    }

    NV12CodecData()
        : CodecData(2)
    {
    }
};
