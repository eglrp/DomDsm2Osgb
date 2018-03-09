#pragma once
#include <vector>
#include <osg/Vec3d>
#include "domDsmToOsgbDlg.h"

class CDataConvertor
{
public:
	void startProcess();

	void startProduce();

	static void produce(void* ptr);

	std::string Tiff2Jpg(std::string inputFile, std::string tifName);

	int produceDsmDomOsgb(/*std::string inputFilePath, std::string strDomTitle, std::string strDsmTitle, int xPart, int yPart*/);

	CdomDsmToOsgbDlg* produceDlg;
};