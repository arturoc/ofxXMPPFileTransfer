#pragma once

#include "ofMain.h"
#include "ofxXMPPFileTransfer.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void onFileTransferReceived(ofxXMPPJingleFileInitiation & fileInitialization);
		void onFileReceivedCorrectly(ofxXMPPFileReceive & xmppfile);
		void onFileSavedCorrectly(ofxXMPPFileReceive & file);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
		
		ofxXMPP xmpp;
		ofxXMPPFileTransfer xmppFileTransfer;
		ofRectangle sendButton;
		int selectedFriend;
		ofTrueTypeFont font;
};
