#include "LineInfo.h"

void sb::release( sb::LineInfo* lineInfo )
{
}

<<<<<<< HEAD
void sb::create( sb::LineInfo* lineInfo, const sb::Line& line )
{
	lineInfo->line = line;
	lineInfo->angle = line.getAngleWithOx();
	lineInfo->length = line.getLength();
	lineInfo->middlePoint = (line.getStartingPoint() + line.getEndingPoint()) * 0.5;
}
=======
void sb::LineInfo::setAverageColor( const cv::Vec3b& averageColor ) { _averageColor = averageColor; }

>>>>>>> master
