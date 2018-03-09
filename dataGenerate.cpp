#include "stdafx.h"
#include "dataGenerate.h"
#include "gdal_alg.h"
#include <ogr_spatialref.h>
#include <gdalwarper.h>
#include "gdal.h"
#include "cpl_conv.h"
#include "gdal_priv.h"
#include<osg/Node>
#include<osg/Geode>
#include<osg/Group>
#include<osgDB/ReadFile>
#include<osgDB/WriteFile>
#include<osg/PolygonMode>
#include<osg/MatrixTransform>
#include<osg/ComputeBoundsVisitor>
#include<osg/shapeDrawable>
#include<osg/Texture2D>
#include<osg/TexGen>
#include<osg/TexEnv>
#include <osgUtil/DelaunayTriangulator>
#include<osgViewer/Viewer>
#include<osg/LineWidth>
#include<osg/CullFace>
#include <regex>
#include <algorithm>
#include <osgDB/WriteFile>

using namespace std;
using namespace osg;
using namespace osgDB;

void CDataConvertor::startProduce()
{
	startProcess();
}

void CDataConvertor::startProcess()
{
	_beginthread(&produce, 0, this);
}

void CDataConvertor::produce(void* ptr)
{
	CDataConvertor* convertor = (CDataConvertor*)ptr;
	convertor->produceDsmDomOsgb();
}

int CDataConvertor::produceDsmDomOsgb(/*string inputFilePath, string strDomTitle, string strDsmTitle, int xPart, int yPart*/)
{
	std::string inputFilePath = produceDlg->filepath;
	std::string strDomTitle = produceDlg->domfile;
	std::string strDsmTitle = produceDlg->demfile;
	int xPart = produceDlg->xPart;
	int yPart = produceDlg->yPart;
	produceDlg->m_progress.SetRange(0, 1000 * xPart * yPart + 1);
	produceDlg->m_progress.SetPos(1);
	produceDlg->m_progress.ShowWindow(SW_SHOW);
	////获取影像
	GDALAllRegister();

	if (strDomTitle.size() == 0)
	{
		AfxMessageBox("Tiff影像路径错误！");
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		return 0;
	}

	std::string imageFileName = Tiff2Jpg(inputFilePath, strDomTitle);
	std::string domFileName = inputFilePath + strDomTitle;
	GDALDataset* poDataSet = (GDALDataset *)GDALOpen(domFileName.c_str(), GA_ReadOnly);

	if (poDataSet == nullptr)
	{
		AfxMessageBox("Tiff影像打开失败！");
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		return 0;
	}

	int wDom = poDataSet->GetRasterXSize();
	int hDom = poDataSet->GetRasterYSize();
	int cntDom = poDataSet->GetRasterCount();

	int totalSize = wDom * hDom * cntDom;

	unsigned char* pData = new unsigned char[totalSize];
	memset(pData, 0, sizeof(unsigned char)* totalSize);
	poDataSet->RasterIO(GF_Read, 0, 0, wDom, hDom, pData, wDom, hDom, GDT_Byte, cntDom, NULL, 0, 0, 0);

	osg::ref_ptr<osg::Image> image = readImageFile(imageFileName);
	if (!image)
	{
		AfxMessageBox("JPG影像打开失败！");
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		return 0;
	}

	osgDB::writeImageFile(*image, imageFileName);

	string dsmFileName = inputFilePath + strDsmTitle;
	GDALDataset* dsmDataSet = (GDALDataset *)GDALOpen(dsmFileName.c_str(), GA_ReadOnly);

	if (dsmDataSet == NULL)
	{
		AfxMessageBox("Tiff影像打开失败！");
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		return 0;
	}

	int wDsm = dsmDataSet->GetRasterXSize();
	int hDsm = dsmDataSet->GetRasterYSize();
	int cntDsm = dsmDataSet->GetRasterCount();

	double xRes = 0; double xRot = 0; double yRot = 0; double yRes = 0; double leftFull = 0; double topFull = 0;


	int pos = strDsmTitle.rfind('.');
	string strDsmCoord = strDsmTitle.substr(0, pos + 1) + "tfw";
	string dsmCoordName = inputFilePath + strDsmCoord;
	FILE* fpDsm = fopen(dsmCoordName.c_str(), "r");
	if (!fpDsm)
	{
		AfxMessageBox(_T("未发现dem的tfw文件，生成失败"));
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		GDALClose(dsmDataSet);
		return 0;
	}
	fscanf(fpDsm, "%lf\n%lf\n%lf\n%lf\n%lf\n%lf\n", &xRes, &xRot, &yRot, &yRes, &leftFull, &topFull);
	fclose(fpDsm);

	//纹理坐标
	double xDRes = 0; double xDRot = 0; double yDRot = 0; double yDRes = 0; double dLeftFull = 0; double dTopFull = 0;

	pos = strDomTitle.rfind('.');
	string strDomCoord = strDomTitle.substr(0, pos + 1) + "tfw";
	string domCoordName = inputFilePath + strDomCoord;
	FILE* fpDom = fopen(domCoordName.c_str(), "r");
	if (!fpDom)
	{
		AfxMessageBox("未发现dom的tfw文件，生成失败");
		produceDlg->m_progress.ShowWindow(SW_HIDE);
		GDALClose(dsmDataSet);
		return 0;
	}
	fscanf(fpDom, "%lf\n%lf\n%lf\n%lf\n%lf\n%lf\n", &xDRes, &xDRot, &yDRot, &yDRes, &dLeftFull, &dTopFull);
	fclose(fpDom);

	produceDlg->GetDlgItem(IDOK)->EnableWindow(FALSE);
	int wsPart = wDsm / xPart + 1;
	int hsPart = hDsm / yPart + 1;
	int percent = 1;
	for (int nY = 0; nY < yPart; nY++)
	{
		for (int nX = 0; nX < xPart; nX++)
		{
			percent = (nY * yPart + nX) * 1000 + 1;
			produceDlg->m_progress.SetPos(percent);
			wsPart = wDsm / xPart + 1;
			hsPart = hDsm / yPart + 1;
			if (nY == (yPart - 1))
			{
				hsPart = hsPart - 1 + (hDsm % (hsPart - 1));
			}
			if (nX == (xPart - 1))
			{
				wsPart = wsPart - 1 + (wDsm % (wsPart - 1));
			}
			ref_ptr<Geode> geode = new Geode;

			int dsmSize = wsPart * hsPart * cntDsm;
			double* dsmData = new double[dsmSize];
			memset(dsmData, 0, dsmSize * sizeof(double));
			dsmDataSet->RasterIO(GF_Read, nX *  int(wDsm / xPart), nY * int(hDsm / yPart), wsPart, hsPart, dsmData, wsPart, hsPart, GDT_Float64, cntDsm, NULL, 0, 0, 0);

			pos = strDomTitle.rfind('.');
			stringstream ss;
			ss << (nY * xPart + nX);


			double left = leftFull + nX * int(wDsm / xPart) * xRes;
			double top = topFull + nY * int(hDsm / yPart) * yRes;
			double dLeft = dLeftFull;
			double dTop = dTopFull;

			double xTrans = left;
			double yTrans = top;

			dLeft = dLeft - left;
			dTop = dTop - top;

			double left1 = left;
			double top1 = top;

			left = 0;
			top = 0;

			ref_ptr<Vec3Array> coord = new Vec3Array;

			for (size_t i = 0; i < hsPart; i++)
			{
				for (size_t j = 0; j < wsPart; j++)
				{
					double h = dsmData[i * wsPart + j];
					if (h < -100) h = 0;

					if (h != 0)
					{
						//计算三维坐标
						double x = left + j * xRes;
						double y = top + i * yRes;

						coord->push_back(Vec3d(x, y, h));
					}
				}
			}

			ref_ptr<Geometry> geom = new Geometry;
			geom->setVertexArray(coord);
			geode->addDrawable(geom);

			//创建Delaunay三角网对象
			ref_ptr<osgUtil::DelaunayTriangulator> dt = new osgUtil::DelaunayTriangulator(coord.get());
			dt->setInputPointArray(coord);
			//10%
			produceDlg->m_progress.SetPos(percent+100);
			//生成三角网
			dt->triangulate();
			//90%
			produceDlg->m_progress.SetPos(percent+900);
			geom->addPrimitiveSet(dt->getTriangles());

			ref_ptr<Vec2Array> textCoord = new Vec2Array;
			ref_ptr<Vec3Array> verts = dynamic_cast<Vec3Array*> (geom->getVertexArray());

			for (int i = 0; i < verts->size(); i++)
			{
				double x = verts->at(i).x();
				double y = verts->at(i).y();

				//计算二维坐标
				double xCoord = (x - dLeft) / xDRes;
				double yCoord = hDom - (y - dTop) / yDRes;
				double xRatio = xCoord / wDom;
				double yRatio = yCoord / hDom;

				textCoord->push_back(Vec2d(xRatio, yRatio));
			}

			image->setFileName(imageFileName);
			ref_ptr<Texture2D> texture = new Texture2D();
			int w = image->s();
			int h = image->t();
			texture->setTextureSize(w, h);
			texture->setInternalFormat(GL_RGBA);
			texture->setImage(image.get());

			geom->setTexCoordArray(0, textCoord.get());

			//样式设置
			ref_ptr<StateSet> state = new StateSet;
			state->setTextureAttributeAndModes(0, texture.get(), StateAttribute::ON);

			geom->setStateSet(state.get());

			ref_ptr<Options> spOptions = new Options;
			spOptions->setPluginStringData("WriteImageHint", "IncludeData");

			osg::Vec3d p = geode->getBound().center();


			ref_ptr<MatrixTransform> trans = new MatrixTransform;
			osg::Matrix mat;
			mat.setTrans(osg::Vec3d(xTrans, yTrans, 0));
			trans->setMatrix(mat);
			trans->addChild(geode);
			;
			std::string modelNameEx = inputFilePath + "domDemModel" + ss.str() + ".osg";
			osgDB::writeNodeFile(*trans, modelNameEx.c_str(), spOptions.get());
			delete[]dsmData;
			//100%
			produceDlg->m_progress.SetPos(percent+999);
		}
	}
	delete[]pData;
	char msg[255];
	sprintf_s(msg,"生成完毕,输出在%s下!",produceDlg->filepath);
	AfxMessageBox(CString(msg));
	produceDlg->GetDlgItem(IDOK)->EnableWindow(TRUE);
	produceDlg->m_progress.ShowWindow(SW_HIDE);
	return 1;
}

std::string CDataConvertor::Tiff2Jpg(std::string inputFile, std::string tifName)
{
	string pDstImgFileName;

	unsigned char* pImageData;
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

	string dsmFileName = inputFile + tifName;
	GDALDataset* dsmDataSet = (GDALDataset *)GDALOpen(dsmFileName.c_str(), GA_ReadOnly);
	if (dsmDataSet == nullptr)
	{
		return "error";
	}
	GDALDriver *pMemDriver = NULL;
	pMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");
	if (pMemDriver == NULL) { return "error"; }

	int width = dsmDataSet->GetRasterXSize();
	int height = dsmDataSet->GetRasterYSize();
	int nChannels = dsmDataSet->GetRasterCount();
	GDALDataset * pMemDataSet = pMemDriver->Create("", width, height, nChannels, GDT_Byte, NULL);
	GDALRasterBand *pBand = NULL;
	int nLineCount = width * nChannels;

	int totalSize = height * nLineCount;
	pImageData = new unsigned char[totalSize];
	memset(pImageData, 0, sizeof(unsigned char)* totalSize);
	if (nChannels == 1)
	{
		pBand = dsmDataSet->GetRasterBand(1);
		pBand->RasterIO(GF_Read, 0, 0, width, height, pImageData, width, height, GDT_Byte, 0, 0);
	}
	else if (nChannels == 3)
	{
		for (int i = 1; i <= nChannels; ++i)
		{
			//GDAL内band存储顺序为RGB，需要转换为我们一般的BGR存储，即低地址->高地址为:B G R  
			unsigned char *pImageOffset = (pImageData + i - 1);
			GDALRasterBand* pBand = dsmDataSet->GetRasterBand(nChannels - i + 1);

			pBand->RasterIO(
				GF_Read,
				0, 0,
				width, height,
				pImageOffset,
				width, height,
				GDT_Byte,
				3,
				0);
		}
	}

	unsigned char *ptr1 = (unsigned char *)pImageData;
	for (int i = 1; i <= nChannels; i++)
	{
		pBand = pMemDataSet->GetRasterBand(nChannels - i + 1);
		pBand->RasterIO(GF_Write,
			0,
			0,
			width,
			height,
			ptr1 + i - 1,
			width,
			height,
			GDT_Byte,
			nChannels,
			nLineCount);
	}

	GDALDriver *pDstDriver = NULL;
	pDstDriver = (GDALDriver *)GDALGetDriverByName("JPEG");
	if (pDstDriver == NULL) { return false; }

	pDstImgFileName = inputFile + tifName.substr(0, tifName.find_last_of('.')) + string(".jpg");
	pDstDriver->CreateCopy(pDstImgFileName.c_str(), pMemDataSet, FALSE, NULL, NULL, NULL);

	GDALClose(pMemDataSet);
	return pDstImgFileName;
}