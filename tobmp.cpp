/*
* =====================================================================================
*
*       Filename:  tobmp.cpp
*    Description: jpg 2 16-565 bmp
*        Version:  1.0.1
*        Created:  2015-05-08 14:01:38
*         Author:  Deep Lee
* =====================================================================================
*/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
extern "C"{
#include "jpeglib.h"
};


namespace Lee
{

    FILE *input_file;
    FILE *output_file;

#pragma pack(push, 1)
    struct bmp_fileheader
    {
        unsigned short    bfType;        //若不对齐，这个会占4Byte
        unsigned int      bfSize;
        unsigned short    bfReverved1;
        unsigned short    bfReverved2;
        unsigned int      bfOffBits;
    };


    struct bmp_infoheader
    {
        unsigned int     biSize;
        unsigned int     biWidth;
        unsigned int     biHeight;
        unsigned short   biPlanes;
        unsigned short   biBitCount;
        unsigned int     biCompression;
        unsigned int     biSizeImage;
        unsigned int     biXPelsPerMeter;
        unsigned int     biYpelsPerMeter;
        unsigned int     biClrUsed;
        unsigned int     biClrImportant;
    };

    struct bmp_rgbmask
    {
        unsigned int rmask;
        unsigned int gmask;
        unsigned int bmask;
    };

    struct bmp_RGBQUAD {
        unsigned char rgbBlue;
        unsigned char rgbGreen;
        unsigned char rgbRed;
        unsigned char rgbReserved;
    };
#pragma pack(pop)

    void write_bmp_header(j_decompress_ptr cinfo)
    {
        struct bmp_fileheader bfh;
        struct bmp_infoheader bih;
        struct bmp_rgbmask    brm;
        struct bmp_RGBQUAD    brq;
        unsigned int width;
        unsigned int height;
        unsigned int depth;
        unsigned int headersize;
        unsigned int filesize;
        unsigned int datasize;
        unsigned char widthAlignBytes;

        width=cinfo->output_width;
        height=cinfo->output_height;
        depth=cinfo->output_components;

         if (depth==1)
        {
            headersize=14+40+256*4;
            filesize=headersize+width*height;
        }

        if (depth==3)
        {
            headersize=14+40+16;
            filesize=headersize+width*height*depth*2/3;
        }

        memset(&bfh, 0, sizeof(struct bmp_fileheader));
        memset(&bih, 0, sizeof(struct bmp_infoheader));
        memset(&brm, 0, sizeof(struct bmp_rgbmask));
        memset(&brq, 0, sizeof(struct bmp_RGBQUAD));

        //写入比较关键的几个bmp头参数
        bfh.bfType=0x4D42;
        bfh.bfSize=filesize;
        bfh.bfOffBits=headersize;

        bih.biSize=0x28+0x10;
        bih.biWidth=width;
        bih.biHeight=height;
        bih.biPlanes=1;
        bih.biCompression = 0x03;
        bih.biBitCount=16;
        bih.biXPelsPerMeter = 0x0B13;
        bih.biYpelsPerMeter = 0x0B13;
        bih.biSizeImage=width*height*depth;

        brm.rmask = 0xF800;
        brm.gmask = 0x07E0;
        brm.bmask = 0x001F;


        fwrite(&bfh, sizeof(struct bmp_fileheader), 1, output_file);
        fwrite(&bih, sizeof(struct bmp_infoheader), 1, output_file);
        fwrite(&brm, sizeof(struct bmp_rgbmask), 1, output_file);
        fwrite(&brq, sizeof(struct bmp_RGBQUAD), 1, output_file);

        // printf("filesize:%d\nwidth:%d\nheight%d\n", filesize, width, height);

        if (depth==1)        //灰度图像要添加调色板
        {
            unsigned char *platte;
            platte=new unsigned char[256*4];
            unsigned char j=0;
            for (int i=0;i<1024;i+=4)
            {
                platte[i]=j;
                platte[i+1]=j;
                platte[i+2]=j;
                platte[i+3]=0;
                j++;
            }
            fwrite(platte,sizeof(unsigned char)*1024,1,output_file);
            delete[] platte;
        }
    }


    void write_bmp_data(j_decompress_ptr cinfo,unsigned char *src_buff)
    {
        unsigned char *dst_width_buff;
        unsigned char *point;
        unsigned char widthAlignBytes;

        unsigned short writebuff;
        unsigned long width;
        unsigned long datasize;
        unsigned long height;
        unsigned short depth;
        unsigned char *r5g6b5;
        unsigned int count = 0;

        width=cinfo->output_width;
        height=cinfo->output_height;
        depth=cinfo->output_components;
        widthAlignBytes = ((width * 16 + 31) & ~31) / 8;
        datasize = widthAlignBytes * height;

        dst_width_buff = new unsigned char[width*depth];
        memset(dst_width_buff, 0, sizeof(unsigned char)*width*depth);
        r5g6b5 = new unsigned char[width * 2];
        memset(r5g6b5, 0, sizeof(unsigned char)*width * 2);

        point=src_buff+width*depth*(height-1);    //倒着写数据，bmp格式是倒的，jpg是正的

        for (unsigned long i=0;i<height;i++)
        {
            for (unsigned long j = 0, count = 0; j < width * depth ; j += depth, count += 2)
            {
                if (depth==1)        //处理灰度图
                {
                    dst_width_buff[j]=point[j];
                }


                if (depth==3)        //处理彩色图
                {
                    dst_width_buff[j+2]=point[j+0];
                    dst_width_buff[j+1]=point[j+1];
                    dst_width_buff[j+0]=point[j+2];

                    writebuff = ((dst_width_buff[j+2] << 8) & 0xF800)  |
                                ((dst_width_buff[j+1] << 3) & 0x07E0)  |
                                ((dst_width_buff[j+0] >> 3) & 0X1F);
                    // printf("%0x, %0x, %0x\n", dst_width_buff[j+2], dst_width_buff[j+1], dst_width_buff[j+0]);
                    // printf("%0x\n", writebuff);
                    r5g6b5[count + 1] = (writebuff & 0xFF00) >> 8;
                    r5g6b5[count + 0] = (writebuff & 0x00FF);
                    // printf("%0x%0x\n", r5g6b5[count], r5g6b5[count+1]);
                }
            }
            point -= width*depth;
            fwrite(r5g6b5, sizeof(unsigned char)*width*2, 1, output_file);    //一次写一行
//            fwrite(dst_width_buff, sizeof(unsigned char)*width*depth, 1, output_file);    //一次写一行
        }
        delete[]dst_width_buff;
    }


    void analyse_jpeg()
    {
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        JSAMPARRAY buffer;
        unsigned char *src_buff;
        unsigned char *point;


        cinfo.err=jpeg_std_error(&jerr);    //一下为libjpeg函数，具体参看相关文档
        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo,input_file);
        jpeg_read_header(&cinfo,TRUE);
        jpeg_start_decompress(&cinfo);


        unsigned long width=cinfo.output_width;
        unsigned long height=cinfo.output_height;
        unsigned short depth=cinfo.output_components;


        src_buff=new unsigned char[width*height*depth];
        memset(src_buff,0,sizeof(unsigned char)*width*height*depth);


        buffer=(*cinfo.mem->alloc_sarray)
            ((j_common_ptr)&cinfo,JPOOL_IMAGE,width*depth,1);


        point=src_buff;
        while (cinfo.output_scanline<height)
        {
            jpeg_read_scanlines(&cinfo,buffer,1);    //读取一行jpg图像数据到buffer
            memcpy(point,*buffer,width*depth);    //将buffer中的数据逐行给src_buff
            point+=width*depth;            //一次改变一行
        }


        write_bmp_header(&cinfo);            //写bmp文件头
        write_bmp_data(&cinfo,src_buff);    //写bmp像素数据

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        delete[] src_buff;
    }
}

int main()
{
    Lee::input_file=fopen("lena.jpg","rb");
    Lee::output_file=fopen("lena.bmp","wb");


    Lee::analyse_jpeg();


    fclose(Lee::input_file);
    fclose(Lee::output_file);


    return 0;
}
