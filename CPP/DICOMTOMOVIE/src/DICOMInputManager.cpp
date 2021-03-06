/*******************************************************************************
 *  Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 *  The contents of this file are subject to the Mozilla Public License
 *  Version 1.1 (the "License"); you may not use this file except in
 *  compliance with the License. You may obtain a copy of the License at
 *  http://www.mozilla.org/MPL/
 *
 *  Software distributed under the License is distributed on an "AS IS"
 *  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 *  License for the specific language governing rights and limitations
 *  under the License.
 *
 *  The Original Code is ICMA
 *
 *  The Initial Developer of the Original Code is University of Auckland,
 *  Auckland, New Zealand.
 *  Copyright (C) 2007-2010 by the University of Auckland.
 *  All Rights Reserved.
 *
 *  Contributor(s): Jagir R. Hussan
 *
 *  Alternatively, the contents of this file may be used under the terms of
 *  either the GNU General Public License Version 2 or later (the "GPL"), or
 *  the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 *  in which case the provisions of the GPL or the LGPL are applicable instead
 *  of those above. If you wish to allow use of your version of this file only
 *  under the terms of either the GPL or the LGPL, and not to allow others to
 *  use your version of this file under the terms of the MPL, indicate your
 *  decision by deleting the provisions above and replace them with the notice
 *  and other provisions required by the GPL or the LGPL. If you do not delete
 *  the provisions above, a recipient may use your version of this file under
 *  the terms of any one of the MPL, the GPL or the LGPL.
 *
 * "2014"
 *******************************************************************************/

#ifndef DICOMINPUTMANAGER_CPP_
#define DICOMINPUTMANAGER_CPP_

#include "DICOMInputManager.h"
#include <cctype>
#include "SegmentationAndFittingException.h"

void DICOMInputManager::Initialize() {
	if (!initialized)
	{
		numOfFrames = 0;
		heartRate = 0.0;
		frameTime = 0.0;
		frames = NULL;
		iOwnmyFile = true;
		InputImageType::Pointer readerImg;

		ImageIOType::Pointer gdcmIO = ImageIOType::New();
		SeriesReaderType::Pointer reader = SeriesReaderType::New();
		reader->SetImageIO(gdcmIO);
		//Check if filename is an url
		if (boost::starts_with(inputFile, "http"))
		{
			URLFileDownloader uriFile(workingDir);
			if (userid.length() != 0)
			{
				uriFile.setUserPassword(userid, passwd);
			}
			if (proxy.length() != 0)
			{
				uriFile.setProxy(proxy);
			}
			//Create a short random file name
			std::ostringstream fnx;
			fnx << "Wado" << rand() << ".dcm";
			while (boost::filesystem::exists(boost::filesystem::path(fnx.str()))) {
				fnx.str("");
				fnx << "Wado" << rand() << ".dcm";
			}
			std::string fn(fnx.str());
			
			if (uriFile.loadURLToFile(inputFile, fn) != 0)
			{
				SAFTHROW("Unable to load URL " + inputFile)
			}

			boost::filesystem::path dir(workingDir);
			boost::filesystem::path qfilename = dir / fn;

			myFile = qfilename.string() + "conv.dcm";
			dcmtkutil.decompressDICOM(qfilename.string(), myFile);
			reader->SetFileName(myFile);
			//Update and read file as uriFile goes out of scope and is deleted
			try{
				reader->Update();
			}catch(...){ //Sometimes when the file uses a different encoding
				gdcmIO->SetCompressionType(itk::GDCMImageIO::JPEG2000);
				reader->SetFileName(inputFile);
				reader->Update();
				myFile = inputFile;
				iOwnmyFile = false;
			}
			readerImg = reader->GetOutput();
			dictionary = (*(reader->GetMetaDataDictionaryArray()))[0];
		}
		else
		{
			myFile = inputFile + "conv.dcm";
			dcmtkutil.decompressDICOM(inputFile, myFile);
			reader->SetFileName(myFile);
			try{
				reader->Update();
			}catch(...){ //Sometimes when the file uses a different encoding
				gdcmIO->SetCompressionType(itk::GDCMImageIO::JPEG2000);
				reader->SetFileName(inputFile);
				reader->Update();
				myFile = inputFile;
				iOwnmyFile = false;
			}
			readerImg = reader->GetOutput();
			dictionary = (*(reader->GetMetaDataDictionaryArray()))[0];
		}

		dicomImage = readerImg;

		inputRegion = dicomImage->GetLargestPossibleRegion();

		size = inputRegion.GetSize();

		start = inputRegion.GetIndex();

		//Read the dictionary for DICOM parameters
		typedef itk::MetaDataDictionary DictionaryType;

		DictionaryType::ConstIterator itr = dictionary->Begin();
		DictionaryType::ConstIterator end = dictionary->End();

		typedef itk::MetaDataObject<std::string> MetaDataStringType;

		std::string noOfFramesTag("0028|0008");
		std::string heartRateTag("0018|1088");
		std::string frameTimeTag("0018|1063");
		std::string rwaveVectorTag("0018|6060");
		std::string studyInstanceUIDTag("0020|000d");
		std::string seriesInstanceUIDTag("0020|000e");
		std::string transducerDataTag("0018|5010");
		std::string sopinstanceuidTag("0008|0018");
		std::string rows("0028|0010");
		std::string cols("0028|0011");
		icmaDicom = false;
		while (itr != end)
		{
			itk::MetaDataObjectBase::Pointer entry = itr->second;

			MetaDataStringType::Pointer entryvalue = dynamic_cast<MetaDataStringType *>(entry.GetPointer());
			if (entryvalue)
			{
				std::string tagkey = trim(itr->first);
				std::string tagvalue = entryvalue->GetMetaDataObjectValue();
				std::pair<std::string, std::string> value(tagkey, tagvalue);
				dEntries.push_back(value);
				{
					std::string value = tagvalue;
					boost::replace_all(value, "^", " ");
					if (boost::starts_with(value, "ABI ICMA"))
					{
						icmaDicom = true;
					}
					std::string labelId;
					bool found = itk::GDCMImageIO::GetLabelFromTag(tagkey, labelId);

					if (found && value.length() < 251 && value.length() > 0)
					{
						dicomHeader.insert(std::make_pair(boost::erase_all_copy(labelId, " "), value));
					}

				}
				if (!tagkey.compare(noOfFramesTag))
				{
					numOfFrames = atoi(trim(tagvalue).c_str());
				}
				if (!cycleset)
				{
					if (!tagkey.compare(heartRateTag))
					{
						heartRate = atof(trim(tagvalue).c_str());
					}
					if (!tagkey.compare(frameTimeTag))
					{
						frameTime = atof(trim(tagvalue).c_str());
					}

					if (!tagkey.compare(rwaveVectorTag))
					{
						std::vector<std::string> strs;
						boost::split(strs, tagvalue, boost::is_any_of("\\"));
						for (unsigned int si = 0; si < strs.size(); si++)
						{
							rwavevector.push_back(atof(trim(strs[si]).c_str()));
						}
					}

				}
				else
				{
					heartRate = 0.0;
					frameTime = 0.0;
				}
				if (!tagkey.compare(studyInstanceUIDTag))
				{
					studyUID = trim(tagvalue);
				}
				if (!tagkey.compare(seriesInstanceUIDTag))
				{
					seriesUID = trim(tagvalue);
				}
				if (!tagkey.compare(rows) && m_TargetHeight == 0.0)
				{ //Skip if set
					m_TargetHeight = atoi(trim(tagvalue).c_str());
				}
				if (!tagkey.compare(cols) && m_TargetWidth == 0.0)
				{ //Skip if set
					m_TargetWidth = atoi(trim(tagvalue).c_str());
				}
				if (!tagkey.compare(transducerDataTag))
				{
					TransducerData = trim(tagvalue);
				}
				if (!tagkey.compare(sopinstanceuidTag))
				{
					sopiuid = trim(tagvalue);
				}
			}
			++itr;
		}

		std::stringstream ss;

		if (!cycleset)
		{
			if (heartRate == 0.0)
			{
				ss << "HeartRate Tag<<" << heartRateTag << ">> is missing in file" << inputFile << " ; All frames are considered to form one cycle";
				cycleset = true;
				EDframe = 0;
				ECframe = numOfFrames - 1;
			}
			if (frameTime == 0.0)
			{
				ss << "FrameTime Tag<<" << frameTimeTag << ">> is missing in file" << inputFile << " ; All frames are considered to form one cycle";
				cycleset = true;
				EDframe = 0;
				ECframe = numOfFrames - 1;
			}
		}

		extractFrames(); //Ensure that the frames are extracted to avoid thread conflicts
		initialized = true;
	}
}

DICOMInputManager::DICOMInputManager(DCMTKUtils& DCMtkutil, std::string filename, int numframes) :
		NUMCAPFRAMES(numframes), dcmtkutil(DCMtkutil) {
	passwd = "";
	userid = "";
	proxy = "";
	cycleset = false;
	inputFile = filename;
	m_TargetHeight = 0.0;
	m_TargetWidth = 0.0;
	initialized = false;
	heartRate = 0.0;
	frameTime = 0.0;
	properties = NULL;
	workingDir = itksys::SystemTools::GetCurrentWorkingDirectory();
}

DICOMInputManager::DICOMInputManager(DCMTKUtils& DCMtkutil, std::string filename, std::string dir, int numframes) :
		NUMCAPFRAMES(numframes), dcmtkutil(DCMtkutil) {
	passwd = "";
	userid = "";
	proxy = "";
	cycleset = false;
	inputFile = filename;
	m_TargetHeight = 0.0;
	m_TargetWidth = 0.0;
	workingDir = dir;
	heartRate = 0.0;
	frameTime = 0.0;
	initialized = false;
	properties = NULL;
}

DICOMSliceImageType::Pointer DICOMInputManager::extract2DImageSlice(int plane, int slice) {

	InputImageType::SizeType mySize = size;
	mySize[plane] = 0;

	InputImageType::IndexType myStart = start;
	const unsigned int sliceNumber = slice;
	myStart[plane] = sliceNumber;

	InputImageType::RegionType desiredRegion;
	desiredRegion.SetSize(mySize);
	desiredRegion.SetIndex(myStart);

	ExtractImageFilterType::Pointer filter = ExtractImageFilterType::New();

	filter->SetExtractionRegion(desiredRegion);
	filter->SetDirectionCollapseToIdentity();
	filter->SetInput(dicomImage);
	filter->Update();

	DICOMSliceImageType::Pointer img = filter->GetOutput();

	img->DisconnectPipeline();
	return img;
}

inline int DICOMInputManager::getPulseLength() {
	int length = 0;
	if (!cycleset)
	{
		double cinerate = 1000.0 / frameTime;
		length = (int) ceil(60.0 / heartRate * cinerate);
	}
	else
	{
		length = ECframe - EDframe + 1;
	}
	return length;
}

float *DICOMInputManager::getFrames() {
	if (numOfFrames == 0)
	{
		float * limits = new float[NUMCAPFRAMES];
		limits[0] = 0;
		return limits;
	}
	if (NUMCAPFRAMES == -1 || NUMCAPFRAMES == numOfFrames)
	{
		NUMCAPFRAMES = numOfFrames;
		float * limits = new float[NUMCAPFRAMES];
		for (int i = 0; i < numOfFrames; i++)
			limits[i] = i;

		return limits;
	}
	float * limits = new float[NUMCAPFRAMES];

	if (!cycleset)
	{

		//If rwave vector is available use that information
		if (rwavevector.size() > 1)
		{
			double last = rwavevector[rwavevector.size() - 1]; //Get the last one
			double before = rwavevector[rwavevector.size() - 2]; //Get the one before last one
			limits[0] = before / frameTime;
			limits[NUMCAPFRAMES - 1] = last / frameTime;
		}
		else
		{ //Most likely a philips dicom or HDC without frametime and heartrate
		  //There seems to be a decrepency between the computed number of frames
		  //that need to be stored based on frametime and heart rate
		  //Rather two cycles are stored in the given number of frames in Phillips dicom
			limits[0] = 1;
			limits[NUMCAPFRAMES - 1] = floor(((double) numOfFrames) / 2.0) + 2;
			if (limits[NUMCAPFRAMES - 1] >= numOfFrames)
				limits[NUMCAPFRAMES - 1] = numOfFrames - 1;
		}
		//Evenly sample the remaining frames
		double step = (limits[NUMCAPFRAMES - 1] - limits[0]) / ((double) NUMCAPFRAMES - 2);
		for (int i = 1; i < NUMCAPFRAMES - 1; i++)
		{
			limits[i] = limits[0] + i * step;
		}
	}
	else
	{
		limits[0] = EDframe;
		//Assign the last frame
		limits[NUMCAPFRAMES - 1] = ECframe;
		double step = getPulseLength() / ((double) NUMCAPFRAMES - 1.0);
		for (int i = 1; i < NUMCAPFRAMES - 1; i++)
		{
			limits[i] = limits[i - 1] + step;
		}
	}
	return limits;
}

inline std::string DICOMInputManager::trim(const std::string & s) {
	std::string result(s);
	result.erase(result.find_last_not_of(" ") + 1);
	result.erase(0, result.find_first_not_of(" "));
	return result;
}

std::vector<DICOMSliceImageType::Pointer>* DICOMInputManager::extractFrames() {
	if (frames == NULL)
	{
		if (NUMCAPFRAMES == -1)
			NUMCAPFRAMES = numOfFrames;
		if (numOfFrames == 0)
		{
			NUMCAPFRAMES = 1;
		}
#ifdef debug
		std::cout<<"Number of frames to be extracted is "<<NUMCAPFRAMES<<std::endl;
#endif
		frames = new std::vector<DICOMSliceImageType::Pointer>(NUMCAPFRAMES);
		ExtractImageFilterType::Pointer filter = ExtractImageFilterType::New();
		ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();

		filter->SetDirectionCollapseToIdentity();

		float* slices = getFrames();
		unsigned int myPlane = 2;

		InputImageType::SizeType mySize = size;
		mySize[myPlane] = 0;
		InputImageType::RegionType desiredRegion;
		desiredRegion.SetSize(mySize);
		InputImageType::IndexType myStart = start;

		for (int i = 0; i < NUMCAPFRAMES; i++)
		{
			const unsigned int sliceNumber = round(slices[i]);
			myStart[myPlane] = sliceNumber;
			desiredRegion.SetIndex(myStart);

			filter->SetExtractionRegion(desiredRegion);
			filter->SetInput(dicomImage);
			filter->Update();

			DICOMSliceImageType::Pointer img = filter->GetOutput();
			img->DisconnectPipeline();
			DICOMSliceImageType::SizeType imgSize = img->GetBufferedRegion().GetSize();
			DICOMSliceImageType::SpacingType imgSpacing = img->GetSpacing();

			DICOMSliceImageType::SizeType noutputSize;
			noutputSize[0] = m_TargetWidth;
			noutputSize[1] = m_TargetHeight;

			DICOMSliceImageType::SpacingType outputSpacing;

			for (unsigned int d = 0; d < 2; d++)
			{
				outputSpacing[d] = imgSpacing[d] * ((double) imgSize[d] / (double) noutputSize[d]);
			}

			resample->SetInput(img);
			resample->SetSize(noutputSize);
			resample->SetOutputSpacing(outputSpacing);
			resample->SetTransform(TransformType::New());
			resample->SetInterpolator(InterpolatorType::New());
			resample->UpdateLargestPossibleRegion();

			DICOMSliceImageType::Pointer output = resample->GetOutput();

			outputSpacing[0] = 1.0;
			outputSpacing[1] = 1.0;

			output->SetSpacing(outputSpacing);

			output->DisconnectPipeline();
			frames->at(i) = output;
		}

		delete[] slices;
	}
	return frames;
}

std::string DICOMInputManager::saveFrameToFile(std::string filename, unsigned int fno) {
	SeriesReaderType::DictionaryRawPointer outputDictionary = new SeriesReaderType::DictionaryType;

	typedef itk::MetaDataObject<std::string> MetaDataStringType;

	for (int i = 0; i < dEntries.size(); i++)
	{
		std::pair<std::string, std::string>& value = dEntries[i];
		std::string tagkey = value.first;
		std::string tagvalue = value.second;
		itk::EncapsulateMetaData<std::string>(*outputDictionary, tagkey, tagvalue);
	}

	DICOMSliceImageType::Pointer image = frames->at(fno);

	//Create a sop uid for the image, while keeping the series and study ids same
	gdcm::UIDGenerator sopUid;
	std::string sopInstanceUID = sopUid.Generate();

	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0008|0018", sopInstanceUID);
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0002|0003", sopInstanceUID);

	//Set number of frames to be 1
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0028|0008", std::string("1"));

	DICOMSliceImageType::SizeType ninputSize = image->GetLargestPossibleRegion().GetSize();

	std::ostringstream in1;
	in1 << ninputSize[0];
	//Set rows 0028,0010
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0028,0010", in1.str());
	std::ostringstream in2;
	in2 << ninputSize[1];
	//Set columns 0028,0011
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0028,0011", in2.str());

	//Set pixel spacing 0028,0030
	DICOMSliceImageType::SpacingType ninputSpacing = image->GetSpacing();
	std::ostringstream ims;
	ims << ninputSpacing[0] << "\\" << ninputSpacing[1];
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0028|0030", ims.str());

	//Image position patient
	std::ostringstream impp;
	impp << 0.0 << "\\" << 0.0 << "\\" << 0.0;
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0020|0032", impp.str());

	//Slice location
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0020|1041", std::string("1"));

	//Image Orientation patient
	std::ostringstream iop;
	iop << 1.0 << "\\" << 0.0 << "\\" << 0.0 << "\\" << 0.0 << "\\" << 1.0 << "\\" << 0.0;
	itk::EncapsulateMetaData<std::string>(*outputDictionary, "0020|0037", iop.str());

	SeriesReaderType::DictionaryArrayType output;
	output.push_back(outputDictionary);

	ImageIOType::Pointer gdcmIO = ImageIOType::New();
	DICOMWriterType::Pointer seriesWriter = DICOMWriterType::New();
	seriesWriter->SetInput(image);
	seriesWriter->SetImageIO(gdcmIO);
	seriesWriter->SetFileName(filename);
	seriesWriter->SetMetaDataDictionaryArray(&output);
	seriesWriter->Update();

	delete outputDictionary;
	return sopInstanceUID;

}

std::string DICOMInputManager::getDICOMFile() {
	return myFile;
}

void DICOMInputManager::setEDECframes(unsigned int ed, unsigned int ec) {
	cycleset = true;
	EDframe = ed;
	ECframe = ec;
}

DICOMInputManager::~DICOMInputManager() {
	frames->clear();
	delete frames;
	//Delete the decompressed dicom file
	if(iOwnmyFile)
		remove(myFile.c_str());
}

void DICOMInputManager::saveFrameAsJpeg(std::string filename, unsigned int fno) {
	DICOMSliceImageType::Pointer image = frames->at(fno);
	ImageFileWriterType::Pointer writer = ImageFileWriterType::New();
	writer->SetInput(image);
	writer->SetFileName(filename);
	writer->Update();
}

std::string DICOMInputManager::getDecompressedDicomFile() {
	return myFile;
}

int DICOMInputManager::rWaveFrame() {
	//If rwave vector is available use that information
	if (rwavevector.size() > 1)
	{
		double last = rwavevector[rwavevector.size() - 1]; //Get the last one
		double before = rwavevector[rwavevector.size() - 2]; //Get the one before last one
		return ((int) floor(before / frameTime));

	}
	else
	{ //Most likely a philips dicom
	  //There seems to be a decrepency between the computed number of frames
	  //that need to be stored based on frametime and heart rate
	  //Rather two cycles are stored in the given number of frames in Phillips dicom
		return 1;
	}
}

void DICOMInputManager::saveAttributesToFile(std::string filename) {
	//std::map<std::string, std::string> header = properties->getProperties();
	std::map<std::string, std::string> header = dicomHeader;
	std::map<std::string, std::string>::iterator start = header.begin();
	const std::map<std::string, std::string>::iterator end = header.end();

	std::ofstream out(filename.c_str(), std::ios::out);
	while (start != end)
	{
		out << start->first << " = " << start->second << std::endl;
		++start;
	}
	//Add details regarding the chosen cardiac cycle
	if (numOfFrames > 0)
	{
		float* frames = getFrames();
		int* rWaveVector = getRWaveMarkers();
		out << "ICMACYCLESTARTFRAME = " << round(frames[0]) << std::endl;
		out << "ICMACYCLEENDFRAME = " << round(frames[NUMCAPFRAMES - 1]) << std::endl;
		out << "TOTALMOVIEFRAMES =" << NUMCAPFRAMES << std::endl;
		out << "MOVIEFRAMERATE =" << (int) getFrameRate() << std::endl;
		out << "MOVIETIME =" << NUMCAPFRAMES / ((int) getFrameRate()) << " secs" << std::endl;
		out << "RWAVESTART = " << rWaveVector[0] << std::endl;
		out << "RWAVEEND = " << rWaveVector[1] << std::endl;
		out.close();
		delete[] rWaveVector;
		delete[] frames;
	}
	else
	{
		out << "DICOMTYPE=SINGLE" << std::endl;
		out.close();
	}
}

bool DICOMInputManager::isICMAInstance() {
	//return properties->isICMADICOM();
	return icmaDicom;
}

int* DICOMInputManager::getRWaveMarkers() {
	int* rWaveVector = new int[2];
	//If rwave vector is available use that information
	if (rwavevector.size() > 1)
	{
		double last = rwavevector[rwavevector.size() - 1]; //Get the last one
		double before = rwavevector[rwavevector.size() - 2]; //Get the one before last one
		rWaveVector[0] = before / frameTime;
		rWaveVector[1] = last / frameTime;
	}
	else
	{
		//Most likely a philips dicom or HDC without frametime and heartrate
		//There seems to be a decrepency between the computed number of frames
		//that need to be stored based on frametime and heart rate
		//Rather two cycles are stored in the given number of frames in Phillips dicom
		rWaveVector[0] = 1;
		rWaveVector[1] = floor(((double) numOfFrames) / 2.0) + 2;
		if (rWaveVector[1] >= numOfFrames)
			rWaveVector[1] = numOfFrames - 1;
	}

	return rWaveVector;
}

Image3D::Pointer DICOMInputManager::getTargetDicomAsImage3D() {
	NormalizedInputCasterType::Pointer caster = NormalizedInputCasterType::New();
	caster->SetInput(dicomImage);
	caster->Update();
	Image3D::Pointer image = caster->GetOutput();
	image->DisconnectPipeline();
	//Check size

	//Resample Image if size and target does not match
	if (m_TargetHeight != size[1] || m_TargetWidth != size[0])
	{
		Resample3DImageFilterType::Pointer resample = Resample3DImageFilterType::New();
		Image3D::SizeType noutputSize = size;
		noutputSize[0] = m_TargetWidth;
		noutputSize[1] = m_TargetHeight;

		Image3D::SpacingType imgSpacing = dicomImage->GetSpacing();

		Image3D::SpacingType outputSpacing = imgSpacing;

		for (unsigned int d = 0; d < 2; d++)
		{
			outputSpacing[d] = imgSpacing[d] * ((double) size[d] / (double) noutputSize[d]);
		}

		resample->SetInput(image);
		resample->SetSize(noutputSize);
		resample->SetOutputSpacing(outputSpacing);
		resample->SetTransform(Transform3DType::New());
		resample->SetInterpolator(Interpolator3DType::New());
		resample->UpdateLargestPossibleRegion();

		Image3D::Pointer output = resample->GetOutput();

		outputSpacing[0] = 1.0;
		outputSpacing[1] = 1.0;

		output->SetSpacing(outputSpacing);

		output->DisconnectPipeline();
		image = output;

	}

	return image;
}

#endif
