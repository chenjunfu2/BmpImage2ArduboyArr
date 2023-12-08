#include <Windows.h>
#include <stdio.h>
#include <ctype.h>


int main(int argc, char *argv[])
{
	if (argc != 2)//接受1参数，其中参数0固定为本exe名字
	{
		fprintf(stderr, "Use:\n\t%s [Input File Name]\nSuch as:\n\t%s Image.bmp\n", argv[0], argv[0]);
		return -1;
	}

	FILE *fi = fopen(argv[1], "rb");

	if (fi == NULL)
	{
		fprintf(stderr, "Cannot open [%s] in\"rb\"mode\n", argv[1]);
		return -1;
	}

	//读取文件头
	BITMAPFILEHEADER stBmpHead;
	if (fread(&stBmpHead, sizeof(stBmpHead), 1, fi) != 1)
	{
		fprintf(stderr, "Read file [%s] error\n", argv[1]);
		return -1;
	}

	//读取信息头
	BITMAPINFOHEADER stBmpInfo;
	if (fread(&stBmpInfo, sizeof(stBmpInfo), 1, fi) != 1)
	{
		fprintf(stderr, "Read file [%s] error\n", argv[1]);
		return -1;
	}

	//读取调色板
	RGBQUAD stPalette[2];
	if (fread(stPalette, sizeof(stPalette), 1, fi) != 1)
	{
		fprintf(stderr, "Read file [%s] error\n", argv[1]);
		return -1;
	}

	if (stBmpInfo.biSize != sizeof(stBmpInfo))
	{
		fprintf(stderr, "Bmp version is incorrect\n");
		return -1;
	}

	if (stBmpHead.bfType != *(WORD*)&"BM" ||
		stBmpInfo.biBitCount != 1 ||
		stBmpInfo.biCompression != BI_RGB)
	{
		fprintf(stderr, "Unsupported bitmap types, only 1bit uncompressed bitmap is supported\n");
		return -1;
	}

	//移动到数据区
	fseek(fi, stBmpHead.bfOffBits, SEEK_SET);
	
	bool bBottom2Up = true;//默认从左下角
	if (stBmpInfo.biHeight < 0)
	{
		bBottom2Up = false;//数据从左上角开始
		stBmpInfo.biHeight = -stBmpInfo.biHeight;
	}


	//((stBmpInfo.biWidth+7)/8+3)&(~3)->要读取的每行字节数，加上7除以8为了求出宽所占的bit数向上舍入有多少字节，加上3与上取反3为了求出向上舍入到4的倍数字节边界，除以4是因为使用int读取
	LONG lBitMapActualWidth = (((stBmpInfo.biWidth + 7) / 8 + 3) & ~3);
	LONG lBitMapActualHeight = stBmpInfo.biHeight;
	UINT8 *u8MapArr = new UINT8[lBitMapActualWidth * lBitMapActualHeight];//这里用4字节类型存储，所以除以4
	if (fread(u8MapArr, sizeof(u8MapArr[0]), lBitMapActualWidth * lBitMapActualHeight, fi) != lBitMapActualWidth * lBitMapActualHeight)
	{
		fprintf(stderr, "Read file [%s] error\n", argv[1]);
		return -1;
	}

	DWORD dRgbAdd[2] =
	{
		(DWORD)stPalette[0].rgbRed + stPalette[0].rgbGreen + stPalette[0].rgbGreen,
		(DWORD)stPalette[1].rgbRed + stPalette[1].rgbGreen + stPalette[1].rgbGreen,
	};

	bool stBitPalette[2] =
	{
		dRgbAdd[0] > dRgbAdd[1],
		dRgbAdd[1] > dRgbAdd[0],
	};


	//按顺序放入bit数组中
	bool *bBitArr = new bool[stBmpInfo.biWidth * stBmpInfo.biHeight];
	//y*stBmpInfo.biWidth+x

	for (LONG ih = 0, bi = 0; ih < stBmpInfo.biHeight; ++ih)
	{
		LONG hCur = bBottom2Up ? stBmpInfo.biHeight - 1 - ih : ih;

		for (LONG iw = 0; iw < stBmpInfo.biWidth; ++iw)
		{
			bool bit = (u8MapArr[bi / 8] << (bi % 8)) & (0x1 << 7);
			++bi;
			bBitArr[hCur * stBmpInfo.biWidth + iw] = stBitPalette[bit];
		}
		//让bi跳到下一行(下一个4byte边界)
		bi = (bi + 31) & ~31;
	}

	delete[] u8MapArr;

	//for (LONG ih = 0; ih < stBmpInfo.biHeight; ++ih)
	//{
	//	for (LONG iw = 0; iw < stBmpInfo.biWidth; ++iw)
	//	{
	//		printf("%s", bBitArr[ih * stBmpInfo.biWidth + iw] ? "■" : "  ");
	//	}
	//	putchar('\n');
	//}


	/*
		bmp:横向存储，每bit一个像素，图像像素行从左到右对应字节位从左到右，像素数对齐4字节32bit，少于补0，图像列从上到下一次存储，每行结束后开始下一列，图像列高度值为正则从左下角开始存储，为负是左上角
		ard:页面存储，每bit一个像素，图像像素每一列中的每8行为一个页面，这一列中从上到下对应比特位从低到高，然后是下一列，直到这一个页面结束，开始下一个页面，如果这一列不满8行则0填充
	*/

	printf("PROGMEM static const uint8_t image[] =//%ld*%ld\n{\n", stBmpInfo.biWidth, stBmpInfo.biHeight);

	LONG lHigh = stBmpInfo.biHeight;
	LONG lWidt = stBmpInfo.biWidth;
	LONG lPage = lHigh / 8 + ((lHigh % 8) != 0);//除以并向上舍入

	LONG lLine = 0;
	for (LONG ip = 0; ip < lPage; ++ip)//一个页面是一列中的8行
	{
		LONG lPageHigh = min((ip + 1) * 8, lHigh);
		for (LONG iw = 0; iw < lWidt; ++iw)//访问每一列
		{
			UINT8 u8CurByte = 0;
			for (LONG ih = ip * 8; ih < lPageHigh; ++ih)//访问每一页面中的每一行
			{
				u8CurByte |= (UINT8)bBitArr[ih * stBmpInfo.biWidth + iw] << (ih - ip * 8);
			}
			
			printf("0x%02X,", u8CurByte);
			if (++lLine == 8)
			{
				putchar('\n');
				lLine = 0;
			}
		}
	}

	if (lLine != 0)//退出循环前没有刚好换过行则换行一下，否则刚好换过行就无需重复换行
	{
		putchar('\n');
	}
	printf("};");

	delete[] bBitArr;
	fclose(fi);
	return 0;
}


