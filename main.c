#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <sys/time.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>
#include <wrap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

T_VideoBuf tVideoBuf;
int CountTimeInter_ms(struct timeval *pre,struct timeval *next);

void InitVideoBuf()
{
	tVideoBuf.iPixelFormat = V4L2_PIX_FMT_YUYV;
	tVideoBuf.tPixelDatas.iBpp	= 16;
	tVideoBuf.tPixelDatas.iHeight = 240;
	tVideoBuf.tPixelDatas.iWidth  = 320;
	tVideoBuf.tPixelDatas.iLineBytes = tVideoBuf.tPixelDatas.iWidth * tVideoBuf.tPixelDatas.iBpp /8;
	tVideoBuf.tPixelDatas.iTotalBytes = 153600;
	tVideoBuf.tPixelDatas.aucPixelDatas = (unsigned char *) malloc( tVideoBuf.tPixelDatas.iTotalBytes );
}

extern int cli_fd;
extern char c;
/* video2lcd </dev/video0,1,...> */
int main(int argc, char **argv)
{	
	T_VideoBuf tConvertBuf; /* 转换好格式的数据及信息 */
    T_VideoBuf tZoomBuf;	/* 缩放后的数据及信息 */
	T_VideoBuf tFrameBuf;	/* 显示屏的一些信息*/
	int iPixelFormatOfVideo;
	int iPixelFormatOfDisp;
	PT_VideoConvert ptVideoConvert;
	int iLcdWidth;
    int iLcdHeigt;
    int iLcdBpp;

	int iTopLeftX;
    int iTopLeftY;

	PT_VideoBuf ptVideoBufCur;	/* 当前指向的一个 */
	
	int ret ,num=0 , iError ;
//	struct timeval pre,next;

	//		DBG_PRINTF("receive :%d bytes , num = %d , times: %d \n",ret , num , (int)(1000/CountTimeInter_ms(&pre ,&next)));
	//		gettimeofday(&next, NULL);
	//		gettimeofday(&pre, NULL);
	float k;

	
	InitVideoBuf();
	ClientInit();	/* 网络初始化 */
	
	/* 注册显示设备的初始化 */
	DisplayInit();
	SelectAndInitDefaultDispDev("crt");
	GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);
	GetVideoBufForDisplay(&tFrameBuf);
    iPixelFormatOfDisp = tFrameBuf.iPixelFormat;
	/* 录像设备的格式 */
	iPixelFormatOfVideo = tVideoBuf.iPixelFormat;

	/* 转换器的初始化 */
	VideoConvertInit();
	ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    if (NULL == ptVideoConvert)
    {
        DBG_PRINTF("can not support this format convert\n");
        return -1;
    }

    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
	tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp;
	
	DBG_PRINTF("tVideoBuf.tPixelDatas.iTotalBytes = %d \n",tVideoBuf.tPixelDatas.iTotalBytes);
	
	while(1)
	{
		fd_set read_set , write_set;
		FD_ZERO(&read_set); FD_ZERO(&write_set);
		FD_SET(cli_fd , &read_set);
		FD_SET(cli_fd , &write_set);
		select(cli_fd+1 ,&read_set , &write_set ,NULL, NULL);
		if(FD_ISSET(cli_fd , &read_set))
			ret = ClientRecv( tVideoBuf.tPixelDatas.aucPixelDatas , tVideoBuf.tPixelDatas.iTotalBytes );
		if(FD_ISSET(cli_fd , &write_set))
		{
			if(c!=0)
			{
				ret = write(cli_fd , &c , 1 );
				if(ret > 0){}
				c = 0;
			}
		}
		
		num++;


		ptVideoBufCur = &tVideoBuf;
		/* 格式转换 */
		if (iPixelFormatOfVideo != iPixelFormatOfDisp)
		{
			iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
			DBG_PRINTF("Convert %s, ret = %d\n", ptVideoConvert->name, iError);
			if (iError)
			{
				DBG_PRINTF("Convert for %s error!\n", argv[1]);
				return -1;
			}			 
			ptVideoBufCur = &tConvertBuf;
		}
		
		/* 如果图像分辨率大于LCD, 缩放 */
        if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
        {
            /* 确定缩放后的分辨率 */
            /* 把图片按比例缩放到VideoMem上, 居中显示
             * 1. 先算出缩放后的大小
             */
            k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;
            tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;
            if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
            {
                tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
            }
            tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            if (!tZoomBuf.tPixelDatas.aucPixelDatas)
            {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }
            
            PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        }

		iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeigt - ptVideoBufCur->tPixelDatas.iHeight) / 2;

		PicMerge(iTopLeftX, iTopLeftY, &ptVideoBufCur->tPixelDatas, &tFrameBuf.tPixelDatas);

        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

	}

	return 0;
}

int CountTimeInter_ms(struct timeval *pre,struct timeval *next)
{
	return ((next->tv_sec*1000 + next->tv_usec/1000) - (pre->tv_sec*1000 + pre->tv_usec/1000));
}


