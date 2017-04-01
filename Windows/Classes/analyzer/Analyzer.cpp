﻿#include "Analyzer.h"
#include "../collector/RawContent.h"

int sb::Analyzer::init( const sb::Params& params )
{
	_laneWidth = params.LANE_WIDTH;
	_roadWidth = params.INITIAL_POSITION_OF_RIGHT_LANE - params.INITIAL_POSITION_OF_LEFT_LANE;

	cv::Point cropPosition;
	cropPosition.x = (params.COLOR_FRAME_SIZE.width - params.CROPPED_FRAME_SIZE.width) / 2;
	cropPosition.y = params.COLOR_FRAME_SIZE.height - params.CROPPED_FRAME_SIZE.height;

	_debugFormatter = sb::Formatter( cv::Rect( cropPosition.x, cropPosition.y,
	                                           params.CROPPED_FRAME_SIZE.width, params.CROPPED_FRAME_SIZE.height ),
	                                 params.WARP_SRC_QUAD,
	                                 params.WARP_DST_QUAD,
	                                 params.CONVERT_COORD_COEF,
	                                 params.SEPERATE_ROWS );

	return 0;
}

int sb::Analyzer::analyze( const sb::FrameInfo& frameInfo, sb::RoadInfo& roadInfo ) const
{
	return analyze2( frameInfo, roadInfo );
}

void sb::Analyzer::release() { }

int sb::Analyzer::analyze1( const sb::FrameInfo& frameInfo,
                            sb::RoadInfo& roadInfo ) const
{
	const int N_LINES = static_cast<int>(frameInfo.getRealLineInfos().size());
	const int N_SECTIONS = static_cast<int>(frameInfo.getSectionInfos().size());

	///// <Old-values> /////
	std::vector<cv::Point2d> oldLeftKnots = roadInfo.getLeftKnots();
	std::vector<cv::Point2d> oldRightKnots = roadInfo.getRightKnots();
	std::vector<double> oldAngles( N_SECTIONS, 0 );
	for ( int i = 0; i < N_SECTIONS; i++ ) {
		sb::Line line( oldLeftKnots[i], oldLeftKnots[i + 1] );
		oldAngles[i] = line.getAngleWithOx();
	}
	///// </Old-values> /////

	///// <Ratings> /////
	//* 1) sử dụng hàm cộng với các hằng số cộng cao/thấp tương úng với tính chất thuộc tính
	//* 2) sử dụng hàm nhân cho mỗi thuộc tính đáp ứng
	//* 3) sử dụng các hàm số có đồ thị đúng yêu cầu

	std::vector<double> lineRatings( N_LINES, 0 );
	std::vector<double> sideRatings( N_LINES, 0 );

	// independent ratings in whole frame
	for ( int i = 0; i < N_LINES; i++ ) {
		const sb::LineInfo& realLine = frameInfo.getRealLineInfos()[i];

		//** color
		//lineRatings[i] += (realLine.getAverageColor()[0] + realLine.getAverageColor()[1] + realLine.getAverageColor()[2]) / 3;

	}

	// independent ratings in section
	for ( int i = 0; i < N_SECTIONS; i++ ) {
		const sb::SectionInfo& sectionInfo = frameInfo.getSectionInfos()[i];
		const int n_lines = static_cast<int>(sectionInfo.lines.size());

		for ( int j = 0; j < n_lines; j++ ) {
			const std::pair<int, cv::Vec2d>& sectionLineInfo = sectionInfo.lines[j];
			const sb::LineInfo realLine = frameInfo.getRealLineInfos()[sectionLineInfo.first];

			const int lineIndex = sectionLineInfo.first;
			const double angle = realLine.getAngle();
			const double length = realLine.getLength();
			const double lowerX = sectionLineInfo.second[0];
			const double upperX = sectionLineInfo.second[1];

			//** position and rotation

			if ( abs( lowerX - oldLeftKnots[i].x ) < 15 ) {
				sideRatings[lineIndex] -= 500;
				lineRatings[lineIndex] += 100/*  x length  */;
				if ( abs( angle - oldAngles[i] ) < 15 ) {
					lineRatings[lineIndex] += 1000/*  x length  */;
					sideRatings[lineIndex] -= 1000;
				}
			}
			if ( abs( lowerX - oldRightKnots[i].x ) < 15 ) {
				sideRatings[lineIndex] += 500;
				lineRatings[lineIndex] += 100/*  x length  */;
				if ( abs( angle - oldAngles[i] ) < 15 ) {
					lineRatings[lineIndex] += 1000/*  x length  */;
					sideRatings[lineIndex] += 1000;
				}
			}
		}

	}

	std::vector<double> tempLineRatings( lineRatings );
	std::vector<double> tempSideRatings( sideRatings );

	// dependent ratings
	for ( int i = 0; i < N_SECTIONS; i++ ) { // section
		const sb::SectionInfo& sectionInfo = frameInfo.getSectionInfos()[i];
		const int n_lines = static_cast<int>(sectionInfo.lines.size());

		for ( int j = 0; j < n_lines; j++ ) { // 1st line
			const std::pair<int, cv::Vec2d>& sectionLineInfo1 = sectionInfo.lines[j];
			const sb::LineInfo& realLine1 = frameInfo.getRealLineInfos()[sectionLineInfo1.first];

			int index1 = sectionLineInfo1.first;
			double angle1 = realLine1.getAngle();
			double length1 = realLine1.getLength();
			double lowerX1 = sectionLineInfo1.second[0];
			double upperX1 = sectionLineInfo1.second[1];

			for ( int k = 0; k < n_lines; k++ ) { // 2nd line
				if ( j == k ) continue;

				const std::pair<int, cv::Vec2d>& sectionLineInfo2 = sectionInfo.lines[k];
				const sb::LineInfo& realLine2 = frameInfo.getRealLineInfos()[sectionLineInfo2.first];

				int index2 = sectionLineInfo2.first;
				double angle2 = realLine2.getAngle();
				double length2 = realLine2.getLength();
				double lowerX2 = sectionLineInfo2.second[0];
				double upperX2 = sectionLineInfo2.second[1];

				// same lane
				if ( tempSideRatings[index1] * tempSideRatings[index2] >= 0
					&& abs( angle1 - angle2 ) < 10
					&& abs( abs( lowerX1 - lowerX2 ) - _laneWidth ) < 10 ) {

					lineRatings[index1] = MAX( tempLineRatings[index1], tempLineRatings[index2] ) + 1000;
					lineRatings[index2] = lineRatings[index1];

					if ( tempSideRatings[index1] < 0 || tempSideRatings[index2] < 0 ) {
						sideRatings[index1] = MIN( tempSideRatings[index1], tempSideRatings[index2] ) - 1000;
						sideRatings[index2] = sideRatings[index1];
					}
					else {
						sideRatings[index1] = MAX( tempSideRatings[index1], tempSideRatings[index2] ) + 1000;
						sideRatings[index2] = sideRatings[index1];
					}
				}

				// opposite lane
				if ( tempSideRatings[index1] * tempSideRatings[index2] <= 0
					&& abs( angle1 - angle2 ) < 15
					&& abs( abs( lowerX1 - lowerX2 ) - _roadWidth ) < 20 ) {

					lineRatings[index1] = MAX( tempLineRatings[index1], tempLineRatings[index2] ) + 1000;
					lineRatings[index2] = lineRatings[index1];

					double sideRating = MAX( abs( tempSideRatings[index1] ), abs( tempSideRatings[index2] ) );

					if ( tempSideRatings[index1] < tempSideRatings[index2] ) {
						tempSideRatings[index1] = -sideRating;
						tempSideRatings[index2] = sideRating;
					}
					else if ( tempSideRatings[index1] > tempSideRatings[index2] ) {
						tempSideRatings[index1] = sideRating;
						tempSideRatings[index2] = -sideRating;
					}

				}
			}
		}
	}
	///// </Ratings> /////

	///// <Debug> /////

	// create real image
	const int W = 900;
	const int H = 700;
	cv::Mat realImage( frameInfo.getColorImage().rows + H,
	                   frameInfo.getColorImage().cols + W, CV_8UC3,
	                   cv::Scalar( 0, 0, 0 ) );

	for ( int i = 0; i < N_LINES; i++ ) {
		const auto& line = frameInfo.getRealLineInfos()[i];

		cv::line( realImage,
		          _debugFormatter.convertFromCoord( line.getStartingPoint() ) + cv::Point2d( W / 2, H ),
		          _debugFormatter.convertFromCoord( line.getEndingPoint() ) + cv::Point2d( W / 2, H ),
		          sideRatings[i] < 0 ? cv::Scalar( 0, 0, 255 ) : cv::Scalar( 0, 255, 255 ),
		          1 );
	}
	for ( int i = 0; i < N_SECTIONS; i++ ) {
		cv::line( realImage,
		          _debugFormatter.convertFromCoord( oldLeftKnots[i] ) + cv::Point2d( W / 2, H ),
		          _debugFormatter.convertFromCoord( oldLeftKnots[i + 1] ) + cv::Point2d( W / 2, H ),
		          cv::Scalar( 170, 170, 170 ), 3 );
		cv::line( realImage,
		          _debugFormatter.convertFromCoord( oldRightKnots[i] ) + cv::Point2d( W / 2, H ),
		          _debugFormatter.convertFromCoord( oldRightKnots[i + 1] ) + cv::Point2d( W / 2, H ),
		          cv::Scalar( 170, 170, 170 ), 3 );
	}

	cv::imshow( "Test Analyzer", realImage );
	cv::waitKey();

	// debug each lines
	for ( int i = 0; i < N_LINES; i++ ) {
		// if ( lineRatings[i] < 2000 ) continue;

		const sb::LineInfo& realLine = frameInfo.getRealLineInfos()[i];

		cv::Mat tempImage = realImage.clone();
		cv::line( tempImage,
		          _debugFormatter.convertFromCoord( realLine.getStartingPoint() ) + cv::Point2d( W / 2, H ),
		          _debugFormatter.convertFromCoord( realLine.getEndingPoint() ) + cv::Point2d( W / 2, H ),
		          cv::Scalar( 0, 255, 0 ), 2 );

		std::stringstream stringBuilder;

		stringBuilder << "Line Rating: " << lineRatings[i];
		cv::putText( tempImage,
		             stringBuilder.str(),
		             cv::Point( 20, 15 ),
		             cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar( 0, 255, 255 ), 1 );

		stringBuilder.str( "" );

		stringBuilder << "Side Rating: " << sideRatings[i];
		cv::putText( tempImage,
		             stringBuilder.str(),
		             cv::Point( 20, 35 ),
		             cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar( 0, 255, 255 ), 1 );

		cv::imshow( "Test2", tempImage );
		cv::waitKey();
	}

	///// </Debug> /////

	///// Update /////

	//** situation for no lines
	std::vector<cv::Point2d> leftKnots( oldLeftKnots );
	std::vector<cv::Point2d> rightKnots( oldRightKnots );

	// first couple of knots
	{
		double sumPositionXOfLeftLane = 0;
		double sumPositionXOfRightLane = 0;
		int n1 = 0;
		int n2 = 0;
		for ( int i = 0; i < frameInfo.getSectionInfos().front().lines.size(); i++ ) {
			int lineIndex = frameInfo.getSectionInfos().front().lines[i].first;

			if ( lineRatings[lineIndex] < 2000 ) continue;

			if ( sideRatings[lineIndex] < 0 ) {
				sumPositionXOfLeftLane += frameInfo.getSectionInfos().front().lines[i].second[0];
				n1++;
			}
			if ( sideRatings[lineIndex] > 0 ) {
				sumPositionXOfRightLane += frameInfo.getSectionInfos().front().lines[i].second[0];
				n2++;
			}
		}

		if ( n1 > 0 ) {
			leftKnots[0].x = sumPositionXOfLeftLane / n1;
		}
		if ( n2 > 0 ) {
			rightKnots[0].x = sumPositionXOfRightLane / n2;
		}
	}

	// left knots
	{
		std::vector<double> angles( oldAngles );
		for ( int i = 0; i < N_SECTIONS; i++ ) {
			const sb::SectionInfo& sectionInfo = frameInfo.getSectionInfos()[i];

			double sumRotation = 0;
			int n = 0;

			for ( int j = 0; j < sectionInfo.lines.size(); j++ ) {
				int lineIndex = sectionInfo.lines[j].first;

				if ( lineRatings[lineIndex] < 2000 ) continue;

				sumRotation += frameInfo.getRealLineInfos()[lineIndex].getAngle();
				n++;
			}

			if ( n > 0 ) {
				angles[i] = sumRotation / n;
			}
		}

		for ( int i = 0; i < N_SECTIONS; i++ ) {
			const int upperRow = static_cast<int>(leftKnots[i + 1].y);

			const sb::Line upperLine( cv::Point2d( 0, upperRow ), cv::Point2d( 1, upperRow ) );

			sb::Line line;

			line = sb::Line( angles[i], leftKnots[i] );

			sb::Line::findIntersection( line, upperLine, leftKnots[i + 1] );

			line = sb::Line( angles[i], rightKnots[i] );

			sb::Line::findIntersection( line, upperLine, rightKnots[i + 1] );
		}
	}

	roadInfo.setLeftKnots( leftKnots );
	roadInfo.setRightKnots( rightKnots );

	return 0;
}

int sb::Analyzer::analyze2( const sb::FrameInfo& frameInfo,
                            sb::RoadInfo& roadInfo ) const
{
	const int N_LINES = static_cast<int>(frameInfo.getRealLineInfos().size());
	const int N_SECTIONS = static_cast<int>(frameInfo.getSectionInfos().size());

	///// <Old-values> /////
	std::vector<cv::Point2d> oldLeftKnots = roadInfo.getLeftKnots();
	std::vector<cv::Point2d> oldRightKnots = roadInfo.getRightKnots();
	std::vector<double> oldAngles( N_SECTIONS, 0 );
	for ( int i = 0; i < N_SECTIONS; i++ ) {
		sb::Line line( oldLeftKnots[i], oldLeftKnots[i + 1] );
		oldAngles[i] = line.getAngleWithOx();
	}
	///// </Old-values> /////

	std::vector<std::vector<int>> lineVotes( N_LINES );
	std::vector<std::vector<int>> sideVotes( N_LINES );

	///// <Ratings> //////
	//* Tính toán độ quan trọng bằng cách thêm n vote cho factor có độ quan trọng n

	// independent ratings in whole frame *importance
	/*for ( int i = 0; i < N_LINES; i++ ) {
		const sb::LineInfo& realLine = frameInfo.getRealLineInfos()[i];

		// color *importance
		{
			int
					b = realLine.getAverageColor()[0],
					g = realLine.getAverageColor()[1],
					r = realLine.getAverageColor()[2];
			double colorAvg = (b + g + r) / 3;
			double colorDiff = (abs( b - g ) + abs( b - r ) + abs( g - r )) / 3;

			// color criterias
			if ( colorAvg > 170 && colorDiff < 50 ) {
				colorAvg = std::exp( MIN( colorAvg, 255 ) * 0.018 );
				colorDiff = (-std::log( MIN( colorDiff+0.1, 255 ) * 86.378 ) + 11) * 10;

				//* coef
				double color_scale = 0.6 * colorAvg + (1 - 0.6) * colorDiff;
				lineVotes[i].push_back( static_cast<int>(color_scale) );
			}
		}
	}*/

	// independent ratings with old values
	/*for ( int i = 0; i < N_SECTIONS; i++ ) {
		const sb::SectionInfo& section = frameInfo.getSectionInfos()[i];
		const int n_lines = static_cast<int>(section.lines.size());

		for ( int j = 0; j < n_lines; j++ ) {
			const std::pair<int, cv::Vec2d>& lineInfo = section.lines[j];
			const sb::LineInfo realLine = frameInfo.getRealLineInfos()[lineInfo.first];

			const int lineIndex = lineInfo.first;
			const double lowerX = lineInfo.second[0];
			const double upperX = lineInfo.second[1];
			const double angle = realLine.getAngle();

			// old state
			double positionDiff = abs( lowerX - oldLeftKnots[i].x );
			double angleDiff = abs( angle - oldAngles[i] );

			// old state criterias
			if ( 1 ) {
				
			}

		}
	}*/

	// dependent ratings
	for ( int i = 0; i < N_SECTIONS; i++ ) { // section
		const sb::SectionInfo& section = frameInfo.getSectionInfos()[i];
		const int n_lines = static_cast<int>(section.lines.size());

		for ( int j = 0; j < n_lines; j++ ) { // 1st line
			const std::pair<int, cv::Vec2d>& lineInfo1 = section.lines[j];
			const sb::LineInfo& realLine1 = frameInfo.getRealLineInfos()[lineInfo1.first];

			int index1 = lineInfo1.first;
			double lowerX1 = lineInfo1.second[0];
			double upperX1 = lineInfo1.second[1];
			double angle1 = realLine1.getAngle();
			double length1 = realLine1.getLength();

			for ( int k = 0; k < n_lines; k++ ) { // 2nd line
				if ( j == k ) continue;

				const std::pair<int, cv::Vec2d>& lineInfo2 = section.lines[k];
				const sb::LineInfo& realLine2 = frameInfo.getRealLineInfos()[lineInfo2.first];

				int index2 = lineInfo2.first;
				double lowerX2 = lineInfo2.second[0];
				double upperX2 = lineInfo2.second[1];
				double angle2 = realLine2.getAngle();
				double length2 = realLine2.getLength();

				// same lane 
				{
					double laneDiff = abs( abs( lowerX1 - lowerX2 ) - _laneWidth );
					double angleDiff = abs( angle1 - angle2 );

					if ( angleDiff < 7 && laneDiff < (0.5 * _laneWidth) ) {
						laneDiff = -std::exp( laneDiff * 4.6 / (0.5 * _laneWidth) ) + 101;
						angleDiff = -std::exp( angleDiff * 4.6 / 7 ) + 101;

						double samelane_scale = 0.5 * angleDiff + (1 - 0.5) * laneDiff;

						int importance = 5; // half-of-important
						for ( ; importance > 0; importance-- ) {
							lineVotes[index1].push_back( static_cast<int>(samelane_scale) );
							lineVotes[index2].push_back( static_cast<int>(samelane_scale) );
						}
					}
				}

				// opposite lane 
				{
					double roadDiff = abs( abs( lowerX1 - lowerX2 ) - _roadWidth );
					double angleDiff = abs( angle1 - angle2 );

					if ( angleDiff < 15 && roadDiff < 2 * _laneWidth ) {
						roadDiff = -std::exp( roadDiff * 4.6 / (2 * _laneWidth) ) + 101;
						angleDiff = -std::exp( angleDiff * 4.6 / 15 ) + 101;

						double oppositelane_scale = 0.6 * angleDiff + (1 - 0.6) * roadDiff;

						int importance = 5; // half-of-important
						for ( ; importance > 0; importance-- ) {
							lineVotes[index1].push_back( static_cast<int>(oppositelane_scale) );
							lineVotes[index2].push_back( static_cast<int>(oppositelane_scale) );
						}
					}
				}

			}
		}
	}

	///// <Ratings> //////
	std::vector<int> lineRatings( N_LINES, 0 );

	// shared fields
	int numberOfVotedLines = 0;
	int totalNumberOfVotes = 0;
	int avgNumberOfVotes = 0;
	int totalRating = 0;
	int avgRating = 0;

	for ( const auto& lineVote: lineVotes ) {
		if ( lineVote.empty() ) continue;

		numberOfVotedLines++;
		for ( const auto& rating: lineVote ) {
			totalNumberOfVotes++;
			totalRating += rating;
		}
	}
	avgNumberOfVotes = totalNumberOfVotes / numberOfVotedLines;
	avgRating = totalRating / totalNumberOfVotes;

	// individual line ranking

	for ( int i = 0; i < N_LINES; i++ ) {
		if ( lineVotes[i].empty() ) continue;;

		int thisNumberOfVotes = 0;
		int thisRating = 0;

		for ( const auto& rating: lineVotes[i] ) {
			thisNumberOfVotes++;
			thisRating += rating;
		}

		thisRating /= thisNumberOfVotes;

		lineRatings[i] = ((avgNumberOfVotes * avgRating) + (thisNumberOfVotes * thisRating)) / (avgNumberOfVotes + thisNumberOfVotes);
	}

	///// </Rankings> //////

	///// <Debug> /////
	{

		// create real image
		const int W = 900;
		const int H = 700;
		cv::Mat realImage( frameInfo.getColorImage().rows + H,
		                   frameInfo.getColorImage().cols + W, CV_8UC3,
		                   cv::Scalar( 0, 0, 0 ) );

		for ( int i = 0; i < N_LINES; i++ ) {
			const auto& line = frameInfo.getRealLineInfos()[i];

			cv::line( realImage,
			          _debugFormatter.convertFromCoord( line.getStartingPoint() ) + cv::Point2d( W / 2, H ),
			          _debugFormatter.convertFromCoord( line.getEndingPoint() ) + cv::Point2d( W / 2, H ),
			          lineRatings[i] >= 80 ? cv::Scalar( 0, 255, 0 ) : cv::Scalar( 0, 0, 255 ), 1 );
		}

		/*for ( int i = 0; i < N_SECTIONS; i++ ) {
			cv::line( realImage,
			          _debugFormatter.convertFromCoord( oldLeftKnots[i] ) + cv::Point2d( W / 2, H ),
			          _debugFormatter.convertFromCoord( oldLeftKnots[i + 1] ) + cv::Point2d( W / 2, H ),
			          cv::Scalar( 170, 170, 170 ), 3 );
			cv::line( realImage,
			          _debugFormatter.convertFromCoord( oldRightKnots[i] ) + cv::Point2d( W / 2, H ),
			          _debugFormatter.convertFromCoord( oldRightKnots[i + 1] ) + cv::Point2d( W / 2, H ),
			          cv::Scalar( 170, 170, 170 ), 3 );
		}*/

		cv::imshow( "Test Analyzer", realImage );
		cv::waitKey();

		// debug each lines
		for ( int i = 0; i < N_LINES; i++ ) {
			if ( lineVotes[i].empty() ) continue;

			const sb::LineInfo& realLine = frameInfo.getRealLineInfos()[i];

			cv::Mat tempImage = realImage.clone();
			cv::line( tempImage,
			          _debugFormatter.convertFromCoord( realLine.getStartingPoint() ) + cv::Point2d( W / 2, H ),
			          _debugFormatter.convertFromCoord( realLine.getEndingPoint() ) + cv::Point2d( W / 2, H ),
			          cv::Scalar( 0, 255, 255 ), 2 );

			std::stringstream stringBuilder;

			stringBuilder << "Votes: " << cv::format( cv::Mat( lineVotes[i] ), "C" );
			cv::putText( tempImage,
			             stringBuilder.str(),
			             cv::Point( 20, 15 ),
			             cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar( 0, 255, 255 ), 1 );

			stringBuilder.str( "" );

			stringBuilder << "Rating: " << lineRatings[i];
			cv::putText( tempImage,
			             stringBuilder.str(),
			             cv::Point( 20, 45 ),
			             cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar( 0, 255, 255 ), 1 );

			stringBuilder.str( "" );

			cv::imshow( "Test2", tempImage );
			cv::waitKey();
		}
	}
	///// </Debug> /////

	///// </Ratings> //////

	return 0;
}

int sb::Analyzer::analyze3( const sb::FrameInfo& frameInfo, sb::RoadInfo& roadInfo ) const
{
	// Bước 1: Sử dụng một cửa sổ vừa nhỏ đủ chứa một làn đường, quét ngang phần dưới khung hình
	// Bước 2: Vote cho những đường có khả năng tạo thành 1 làn đường (song song, cách nhau một khoảng gần bằng độ rộng làn đường)
	// Bước 3: Tính điểm đánh giá cho những đường vừa vote
	// Bước 4: Nếu đánh giá đủ cao để xem là làn đường đến bước 5, ngược lại quay lại bước 1 nhưng với phần khung hình cao hơn
	// Bước 5: Trượt cửa sổ chiều của làn đường đó
	// Bước 6: Vừa trượt vừa tìm những đường nối tiếp làn đường đó
	// Bước 7: Tìm càng được càng nhiều đường nối, đánh giá càng cao
	// Bước 8: Nếu điểm đủ cao, xác định đó là 1 trường hợp làn đường có thể
	// Bước 9: Tiếp tục tìm tương tự
	// Bước 10: Nếu tồn tại làn có đánh giá đủ cao thứ 2
	// Bước 11: Xét quan hệ giữa 2 làn tìm đường
	// Bước 12: Nếu các quan hệ hợp lý tạo thành làn đường thì đó là làn thứ hai. Kết thúc
	// Bước 13: Tiếp tục tìm tương tự.
	// Bước 14: Nếu tồn tại làn có đánh giá đủ cao thứ n
	// Bước 15: Xét quan hệ với các làn còn lại
	// Bước 16: Nếu quan hệ hợp lý tạo thành làn đường. Kết thúc.
	// Bước 17: Không tìm được thêm làn nào. Xác định làn tìm được là trái hay phải dựa trên quan hệ với trạng thái cũ
	// Bước 19: Nếu khoogn tìm được làn nào hợp lý cả, sử dụng kết quả tìm được gợi ý làn đường

	return 0;
}
