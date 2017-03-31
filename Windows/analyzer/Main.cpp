#include "../Classes/calculator/Calculator.h"
#include "../Classes/collector/Collector.h"
#include "../Classes/analyzer/Analyzer.h"
#include "../Classes/Timer.h"

#define WINDOW_EGO_VIEW "Ego-View"
#define WINDOW_BIRDEYE_VIEW "Birdeye-View"

int init( sb::Collector& collector,
          sb::Calculator& calculator,
          sb::Analyzer& analyzer,
          const sb::Params& params );

void test( const sb::Calculator& calculator,
           const sb::RawContent& rawContent,
           const sb::FrameInfo& frameInfo,
           const sb::RoadInfo& roadInfo,
           const sb::Params& params );

void release( sb::Collector& collector,
              sb::Calculator& calculator,
              sb::Analyzer& analyzer );

int main()
{
	// Parameters container for every component
	sb::Params params;
	params.load( PARAMS_PATH );

	// Timer for performance test
	sb::Timer timer;

	// Data sent&receive bewteen components
	sb::RawContent rawContent;
	rawContent.create( params );

	sb::FrameInfo frameInfo;
	frameInfo.create( params );

	sb::RoadInfo roadInfo;
	roadInfo.create( params );

	// Main Components
	sb::Collector collector;
	sb::Calculator calculator;
	sb::Analyzer analyzer;

	// Init components
	if ( init( collector, calculator, analyzer, params ) < 0 ) {
		std::cerr << "Init failed." << std::endl;
		return -1;
	}

#ifdef _DEBUG
	cv::namedWindow( WINDOW_EGO_VIEW, CV_WINDOW_AUTOSIZE );
	cv::namedWindow( WINDOW_BIRDEYE_VIEW, CV_WINDOW_AUTOSIZE );
	cv::waitKey();
#endif

	std::vector<std::vector<cv::Point2d>> arrayOfLeftKnots;
	std::vector<std::vector<cv::Point2d>> arrayOfRightKnots;

	timer.reset( "entire_job" );
	while ( true ) {
		timer.reset( "total" );

		timer.reset( "collector" );
		if ( collector.collect( rawContent ) < 0 ) {
			std::cerr << "Collector collects failed." << std::endl;
			break;
		}
		std::cout << "Collector: " << timer.milliseconds( "collector" ) << "ms." << std::endl;

		timer.reset( "calculator" );
		if ( calculator.calculate( rawContent, frameInfo ) < 0 ) {
			std::cerr << "Calculator calculates failed." << std::endl;
			break;
		}
		std::cout << "Calculator: " << timer.milliseconds( "calculator" ) << "ms." << std::endl;

		timer.reset( "analyzer" );
		if ( analyzer.analyze( frameInfo, roadInfo ) ) {
			std::cerr << "Analyzer analyzes failed." << std::endl;
			break;
		}
		std::cout << "Analyzer: " << timer.milliseconds( "analyzer" ) << "ms." << std::endl;

		std::cout
				<< "Executed time: " << timer.milliseconds( "total" ) << ". "
				<< "FPS: " << timer.fps( "total" ) << std::endl;

		arrayOfLeftKnots.push_back( roadInfo.getLeftKnots() );
		arrayOfRightKnots.push_back( roadInfo.getRightKnots() );

#ifdef _DEBUG
		test( calculator, rawContent, frameInfo, roadInfo, params );

		if ( cv::waitKey( MAX( 1, 66 - timer.milliseconds( "total" ) ) ) == KEY_TO_ESCAPE ) break;
#endif
	}

	std::cout << "Average executiton time: " << timer.milliseconds( "entire_job" ) / arrayOfLeftKnots.size() << "ms." << std::endl;

	cv::FileStorage roadInfoStream( "road_info.yaml", cv::FileStorage::WRITE );
	roadInfoStream
		<< "LEFT_KNOTS" << arrayOfLeftKnots
		<< "RIGHT_KNOTS" << arrayOfRightKnots;
	roadInfoStream.release();


	release( collector, calculator, analyzer );
	
	system( "pause" );
	return 0;
}

int init( sb::Collector& collector,
          sb::Calculator& calculator,
          sb::Analyzer& analyzer,
          const sb::Params& params )
{
	if ( collector.init( params ) < 0 ) {
		std::cerr << "Collector init failed." << std::endl;
		return -1;
	}

	if ( calculator.init( params ) < 0 ) {
		std::cerr << "Calculator init failed." << std::endl;
		return -1;
	}

	if ( analyzer.init( params ) < 0 ) {
		std::cerr << "Analyzer init failed." << std::endl;
		return -1;
	}

	return 0;
}

void test( const sb::Calculator& calculator,
           const sb::RawContent& rawContent,
           const sb::FrameInfo& frameInfo,
           const sb::RoadInfo& roadInfo,
           const sb::Params& params )
{
	///// Init image /////
	const int N_SECTIONS = static_cast<int>(frameInfo.getSectionInfos().size());

	const cv::Size FRAME_SIZE = frameInfo.getColorImage().size();

	const cv::Size CAR_SIZE( 90, 120 );

	const cv::Size EXPAND_SIZE( 900, 700 );

	const cv::Point CAR_POSITION( FRAME_SIZE.width / 2,
	                              FRAME_SIZE.height );

	cv::Mat radarImage = cv::Mat::zeros( FRAME_SIZE.height + EXPAND_SIZE.height + CAR_SIZE.height,
	                                     FRAME_SIZE.width + EXPAND_SIZE.width,
	                                     CV_8UC3 );

	///// Calculate lane positions /////

	std::vector<cv::Point2d> leftKnots( N_SECTIONS + 1, cv::Point2d( 0, 0 ) );
	std::vector<cv::Point2d> rightKnots( N_SECTIONS + 1, cv::Point2d( 0, 0 ) );
	
	for ( int i = 0; i < N_SECTIONS + 1; i++ ) {
		leftKnots[i] = calculator.convertFromCoord( roadInfo.getLeftKnots()[i] );
		rightKnots[i] = calculator.convertFromCoord( roadInfo.getRightKnots()[i] );
	}

	// draw lane
	for ( int i = 0; i < N_SECTIONS; i++ ) {
		cv::line( radarImage,
		          leftKnots[i] + cv::Point2d( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
		          leftKnots[i + 1] + cv::Point2d( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
		          cv::Scalar( 255, 255, 255 ), 7 );
		cv::line( radarImage,
		          rightKnots[i] + cv::Point2d( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
		          rightKnots[i + 1] + cv::Point2d( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
		          cv::Scalar( 255, 255, 255 ), 7 );
	}

	// draw vehicle
	cv::rectangle( radarImage,
	               CAR_POSITION - cv::Point( CAR_SIZE.width / 2, 0 ) + cv::Point( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
	               CAR_POSITION + cv::Point( CAR_SIZE.width / 2, CAR_SIZE.height ) + cv::Point( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
	               cv::Scalar( 0, 0, 255 ), -1 );
	cv::rectangle( radarImage,
	               CAR_POSITION - cv::Point( CAR_SIZE.width / 2, 0 ) + cv::Point( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
	               CAR_POSITION + cv::Point( CAR_SIZE.width / 2, CAR_SIZE.height ) + cv::Point( EXPAND_SIZE.width / 2, EXPAND_SIZE.height ),
	               cv::Scalar( 0, 255, 255 ), 4 );

	cv::imshow( WINDOW_EGO_VIEW, rawContent.getColorImage() );

	cv::imshow( WINDOW_BIRDEYE_VIEW, radarImage );
}

void release( sb::Collector& collector,
              sb::Calculator& calculator,
              sb::Analyzer& analyzer )
{
	calculator.release();

	collector.release();

	analyzer.release();
}
