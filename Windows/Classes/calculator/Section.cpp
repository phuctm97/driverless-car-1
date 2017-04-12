#include "Section.h"

void sb::create( sb::Section* section,
								 const cv::Mat& containerBgrImage,
								 const cv::Mat& containerBinImage,
								 const cv::Mat& containerEdgImage,
								 const cv::Rect& rect )
{
	section->bgrImage = containerBgrImage( rect );
	
	section->binImage = containerBinImage( rect );

	section->edgImage = containerEdgImage( rect );

	section->imageRect = rect;

	section->bottomLine = sb::Line( cv::Point2d( 0, rect.y + rect.height - 1 ),
	                                cv::Point2d( 1, rect.y + rect.height - 1 ) );
	section->topLine = sb::Line( cv::Point2d( 0, rect.y ),
	                             cv::Point2d( 1, rect.y ) );
}

void sb::release( sb::Section* section )
{
	section->bgrImage.release();
	section->binImage.release();
	section->edgImage.release();

	for( auto it_blob = section->blobs.begin(); it_blob != section->blobs.end(); ++it_blob ) {
		sb::release( *it_blob );
		delete *it_blob;
	}
	section->blobs.clear();
}

cv::Point sb::convertToContainerSpace( sb::Section* section, const cv::Point& pos )
{
	return cv::Point( section->imageRect.x + pos.x, pos.y + section->imageRect.y );
}

cv::Point2f sb::convertToContainerSpace( sb::Section* section, const cv::Point2f& pos )
{
	return cv::Point2f( section->imageRect.x + pos.x, pos.y + section->imageRect.y );
}
