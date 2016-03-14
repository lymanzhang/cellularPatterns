#include "ofApp.h"
#include "constants.h"

using namespace ofxCv;
using namespace cv;

float w = 750; //750
float h = 1050; //1000

float maxDensity(50);//150 810
float minDensity(10);//30  200

float maxDensity2(16);
float minDensity2(3);
float anisotrophyStr(1.0f/1.5f);

float sizeFallOffExp = .75;

float minThick = 5.0f; //.05 inches rubber
float maxThick = 9.9f; //.1 inches rubber
String imageName = "circles2.png";

int binW, binH, binD, binWH;
vector< vector<int> > bins;
bool isOptimizing = false;
bool record = false;
bool hasMask = true; //if you are using an image to crop the pattern

vector<AnisoPoint2f> nearPts;
AnisoPoint2f(*getAnisoPoint)(const ofVec3f & pt);

Mat imgDist;
Mat imgGradX, imgGradY;

//--------------------------------------------------------------
void ofApp::setup(){
	
	baseImage.load(imageName);
	w = baseImage.getWidth();
	h = baseImage.getHeight();
	ofSetWindowShape(w, h);
	Mat initImg(baseImage.getHeight(), baseImage.getWidth(), CV_8UC1);
	cvtColor(toCv(baseImage), initImg, COLOR_BGR2GRAY);
	//toCv(baseImage).copyTo(initImg);
	//imgGradX = Mat(baseImage.getHeight(), baseImage.getWidth(), CV_32FC1);
	//imgGradY = Mat(baseImage.getHeight(), baseImage.getWidth(), CV_32FC1);
	distanceTransform(initImg, imgDist, CV_DIST_L2, CV_DIST_MASK_PRECISE);
	normalize(imgDist, imgDist);
	Mat tempImg1 = imgDist;
	Mat tempImg2 = imgDist;
	Scharr(tempImg1, imgGradX, CV_32F, 1, 0);
	Scharr(tempImg2, imgGradY, CV_32F, 0, 1);
	toOf(imgDist, distImage);
	
	//minDensity = minDensity2;
	//maxDensity = maxDensity2;

	binW = floor(w / maxDensity) + 1;
	binH = floor(h / maxDensity) + 1;
	//important thing
	//anisotropy function - give it a pt in space and it returns an anisotropic pt
	//getAnisoPtImg - uses image
	//getAnisoPtEdge - edge of the screen
	//getAnisoPtNoise
	//getAnisoPt - distance from a single Pt
	getAnisoPoint = &getAnisoPtSet;// &getAnisoEdge;

	linesMesh.setMode(OF_PRIMITIVE_LINES);
	bins.resize(binW*binH);
	initPts();
	getDistances();
	dualContour();
}

void ofApp::initPts() {
	pts.clear();
	int fail = 0;
	ofVec3f pt;
	int totalTries = 0;
	//density = ofLerp(maxDensity, minDensity, ofClamp(x/200.0,0,1));
	
	while(fail < 5000) {
		pt = ofVec3f(ofRandom(w), ofRandom(h));
		//check mask
		if (!hasMask || imgDist.at<float>((int)pt.y, (int)pt.x) > 0) {
			//density = ofLerp(maxDensity, minDensity, ofClamp(x/200.0,0,1));
			if (addPt(pt)) {
				fail = 0;
				cout << pts.size() << endl;
			}
			else {
				fail++;
			}
		}
	}
}

bool ofApp::addPt(ofVec3f & pt) {
	MyPoint aniPt = getAnisoPoint(pt);

	int sx = (int)(pt.x / maxDensity);
	int sy = (int)(pt.y / maxDensity);
	int sz = (int)(pt.z / maxDensity);
	int minX = max<int>(sx - 1, 0);
	int maxX = min<int>(sx + 1, binW - 1);
	int minY = max<int>(sy - 1, 0);
	int maxY = min<int>(sy + 1, binH - 1);
	int minZ = max<int>(sz - 1, 0);
	int maxZ = min<int>(sz + 1, binD - 1);
	//density = ofLerp(minDensity, maxDensity, ofClamp(1.0-abs(x)/133.0,0,1));

	for (int i = minX; i <= maxX; i++) {
		for (int j = minY; j <= maxY; j++) {
			vector<int> &bin = bins[j*binW + i];

			for (unsigned int index = 0; index < bin.size(); index++) {
				int ind = bin[index];
				MyPoint pt2 = pts[ind];
				float d = metric.distance_square(*aniPt.pt, *pt2.pt, *aniPt.jacobian);
				if (d < 1) return false;
				//d = metric.distance_square(pt2, aniPt);
				//if (d < 1) return false;
			}
		}
	}

	pts.push_back(aniPt);
	vector<int> &bin = bins[sy*binW + sx];
	bin.push_back(pts.size() - 1);
	return true;
}

//--------------------------------------------------------------
void ofApp::update(){
	if (isOptimizing) {
		if (!optThread.isThreadRunning()) {
			isOptimizing = false;
			pts = optThread.pts;
			getDistances();
			dualContour();
			cout << "done" << endl;
		}
		else if (ofGetFrameNum() % 20 == 0) {
			optThread.lock();
			pts = optThread.pts;
			optThread.unlock();
			getDistances();
			dualContour();
		}
	}
}

void ofApp::setupStage2() {
	nearPts = pts;
	getAnisoPoint = &getAnisoPointPts;
	minDensity /= 7;// 8;// = minDensity2;
	maxDensity /= 7;// 8;// = maxDensity2;
	binW = floor(w / maxDensity) + 1;
	binH = floor(h / maxDensity) + 1;

	bins.resize(binW*binH);
	for (auto & bin : bins) {
		bin.clear();
	}

	initPts();
	getDistances();
	dualContour();
}

//--------------------------------------------------------------
void ofApp::draw(){
	
	ofBackground(255);


	std::ostringstream ss;
	ss << "voronoi_dir" << anisotrophyStr << "_cellSz_" << minDensity << "-" << maxDensity << "_" << ofGetTimestampString() << ".pdf";

	

	if (record) ofBeginSaveScreenAsPDF(ss.str());
	//drawPtEllipses();
	//distImage.draw(0,0);
	ofSetColor(0);
	ofNoFill();
	//linesMesh.draw();
	//if (record) {
	for (int i = 0; i < linesMesh.getNumIndices();i+=2) {
		ofVec2f pt = (linesMesh.getVertex(linesMesh.getIndex(i))+ linesMesh.getVertex(linesMesh.getIndex(i + 1)))*.5;
		AnisoPoint2f cellPt = pts[distances[(w*int(pt.y) + int(pt.x))*3].index];
		AnisoPoint2f cellPt2 = pts[distances[(w*int(pt.y) + int(pt.x)) * 3+1].index];
		//stroke
		//get 2 closest cell pts and use their area  (determinant of their jacobian of the size)
		float weight = .2/sqrt(cellPt.jacobian->determinant())+ .2 / sqrt(cellPt2.jacobian->determinant());

		//clamp in between min and max stroke weight
		weight = ofClamp(weight, minThick, maxThick);

		ofSetLineWidth(weight);
		ofDrawLine(linesMesh.getVertex(linesMesh.getIndex(i)), linesMesh.getVertex(linesMesh.getIndex(i+1)));
		
	}
	if (record) {
		record = false;
		ofEndSaveScreenAsPDF();
	}
}

void ofApp::drawPtEllipses() {
	ofMatrix4x4 mat;
	mat.makeIdentityMatrix();
	ofSetColor(0);
	ofNoFill();
	for (auto & pt : pts) {
		ofPushMatrix();
		
		ofTranslate(pt[0],pt[1]);

		Matrix2f transform = pt.jacobian->inverse();
		
		mat(0, 0) = transform(0, 0)*0.5;
		mat(1, 0) = transform(0, 1)*0.5;
		mat(1, 1) = transform(1, 1)*0.5;
		mat(0, 1) = transform(1, 0)*0.5;
		
		ofMultMatrix(mat);
		
		ofDrawCircle(0, 0, 0, 1);
		ofPopMatrix();
	}
}

void ofApp::getDistances() {
	distances.resize(w*h * 3);
	for (int i = 0; i < distances.size(); ++i) distances[i] = IndexDist(0, 9e9);
	if (hasMask) {
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				if (imgDist.at<float>(y, x) == 0) {
					distances[(w*y + x) * 3] = IndexDist(1, 0);
				}
			}
		}
	}
	Vector2f tempPt;
	for (int i = 0; i < pts.size();++i) {
		AnisoPoint2f & pt = pts[i];
		ofPushMatrix();

		Matrix2f transform = pt.jacobian->inverse();
		Vector2f vec(0, 1);
		vec = transform*vec;
		Vector2f vec2(1, 0);
		vec2 = transform*vec2;
		vec(0) = max(abs(vec(0)), abs(vec2(0)));
		vec(1) = max(abs(vec(1)), abs(vec2(1)));
		int minX = max(0, (int) (pt[0] - vec(0)*2));
		int maxX = min((int) w-1, (int)(pt[0] + vec(0)*2));
		int minY = max(0, (int)(pt[1] - vec(1)*2));
		int maxY = min((int)h - 1, (int)(pt[1] + vec(1)*2));
		for (int x = minX; x <= maxX; ++x) {
			for (int y = minY; y <= maxY; ++y) {
				unsigned int index = (w*y + x) * 3;
				tempPt[0] = x;
				tempPt[1] = y;
				float dist = metric.distance_square(*pt.pt, tempPt, *pt.jacobian);
				if (dist < distances[index].dist) {
					distances[index + 2] = distances[index + 1];
					distances[index + 1] = distances[index];
					distances[index].index = i;
					distances[index].dist = dist;
				}
				else if (dist < distances[index + 1].dist) {
					distances[index + 2] = distances[index + 1];
					distances[index + 1].index = i;
					distances[index + 1].dist = dist;
				}
				else if (dist < distances[index + 2].dist) {
					distances[index + 2].index = i;
					distances[index + 2].dist = dist;
				}


				
			}
		}
	}

}

ofVec2f getVoronoiIntersection(ofVec2f p1, ofVec2f p2, float side1A, float side1B, float side2A, float side2B) {
	//float eLen = p1.distance(p2);

	float x = (side1A - side2A) / (side1A - side1B + side2B - side2A);
	x = ofClamp(x, 0, 1);
	return ofVec2f(p1.x + (p2.x - p1.x)*x, p1.y + (p2.y - p1.y)*x);
}

void ofApp::dualContour() {
	linesMesh.clear();
	vector<ofVec2f> edgePts(w*h * 2);
	vector<int> ptIndices(w*h,-1);
	for (int y = 0; y < h - 1; ++y) {
		int wy = w*y;
		for (int x = 0; x < w - 1; ++x) {
			IndexDist p1 = distances[(wy + x)*3];
			IndexDist p2 = distances[(wy + x + 1)*3];
			IndexDist p3 = distances[(wy + x + w)*3];
			if (p1.index != p2.index) {
				edgePts[(wy + x) * 2] = ofVec3f(x+0.5,y);
			}
			if (p1.index != p3.index) {
				edgePts[(wy + x) * 2+1] = ofVec3f(x,y+0.5);
			}
		}
	}
	//NOT PROPERLY DOING BOTTOM AND RIGHT EDGE
	vector<vector<int> > neighbors;
	for (int y = 0; y < h - 2; ++y) {
		int wy = w*y;
		for (int x = 0; x < w - 2; ++x) {
			IndexDist p1 = distances[(wy + x) * 3];
			IndexDist p2 = distances[(wy + x + 1) * 3];
			IndexDist p3 = distances[(wy + x + w) * 3];
			IndexDist p4 = distances[(wy + x + w+1) * 3];
			IndexDist p1b = distances[(wy + x) * 3+1];
			IndexDist p2b = distances[(wy + x + 1) * 3+1];
			IndexDist p3b = distances[(wy + x + w) * 3+1];
			IndexDist p4b = distances[(wy + x + w + 1) * 3+1];
			int numInts = 0;
			ofVec3f center;
			bool conLeft = false;
			bool conUp = false;
			if (p1.index != p2.index) {
				//center += edgePts[(wy + x) * 2];
				center += getVoronoiIntersection(ofVec2f(x, y), ofVec2f(x + 1, y), p1.dist, p2b.dist, p1b.dist, p2.dist);
				numInts++;
				conUp = true;
			}
			if (p1.index != p3.index) {
				//center += edgePts[(wy + x) * 2+1];
				center += getVoronoiIntersection(ofVec2f(x, y), ofVec2f(x, y+1), p1.dist, p3b.dist, p1b.dist, p3.dist);
				numInts++;
				conLeft = true;
			}
			if (p2.index != p4.index) {
				//center += edgePts[(wy + x+1) * 2+1];
				center += getVoronoiIntersection(ofVec2f(x+1, y), ofVec2f(x+1, y + 1), p2.dist, p4b.dist, p2b.dist, p4.dist);
				numInts++;
			}
			if (p3.index != p4.index) {
				//center += edgePts[(wy + x+w) * 2];
				center += getVoronoiIntersection(ofVec2f(x, y+1), ofVec2f(x+1, y + 1), p3.dist, p4b.dist, p3b.dist, p4.dist);
				numInts++;
			}
			if (numInts > 0) {
				center /= numInts;
				linesMesh.addVertex(center);
				int currIndex = linesMesh.getNumVertices() - 1;
				ptIndices[wy + x] = currIndex;
				if (x > 0 && conLeft) {
					linesMesh.addIndex(currIndex);
					linesMesh.addIndex(ptIndices[wy+x-1]);
				}
				if (y > 0 && conUp) {
					linesMesh.addIndex(currIndex);
					linesMesh.addIndex(ptIndices[wy+x-w]);
				}
			}
		}
	}

	vector<bool> processed(linesMesh.getNumVertices(), false);
}

void ofApp::optimize() {
	if (!isOptimizing) {
		optThread.setup(pts);
		optThread.w = w;
		optThread.h = h;
		optThread.minDensity = minDensity;
		optThread.maxDensity = maxDensity;
		isOptimizing = true;
		optThread.startThread(true, true);
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	switch (key) {
	case 'o':
		cout << "optimize" << endl;
		optimize();
		break;
	case 's':
		setupStage2();
		break;
	case 'r':
		record = true;
		break;
	case 'a':
		anisotrophyStr = 1.0 / anisotrophyStr;
		break;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
