#include "ofApp.h"

//#define IN_MEMORY_TRANSFER

//--------------------------------------------------------------
void ofApp::setup(){
	ofXml settings;
	settings.load("settings.xml");
	string server = settings.getValue("server");
	string user = settings.getValue("user");
	string pwd = settings.getValue("pwd");


	xmpp.setShow(ofxXMPPShowAvailable);
	xmpp.setStatus("connnected from ofxXMPP");
	xmpp.connect(server,user,pwd);

	xmppFileTransfer.setup(xmpp);
	xmppFileTransfer.setTransferRate(100);  // sert transfer rate in KBytes per second

	ofAddListener(xmppFileTransfer.fileTransferReceived,this,&ofApp::onFileTransferReceived);
#ifdef IN_MEMORY_TRANSFER
	ofAddListener(xmppFileTransfer.fileReceivedCorrectly,this,&ofApp::onFileReceivedCorrectly);
#else
	ofAddListener(xmppFileTransfer.fileSavedCorrectly,this,&ofApp::onFileSavedCorrectly);
#endif

	ofBackground(ofColor::white);
	selectedFriend = -1;

	font.loadFont(OF_TTF_SANS,24);
}

void ofApp::onFileTransferReceived(ofxXMPPJingleFileInitiation & fileInitialization){
	xmppFileTransfer.acceptToFolderTransfer(fileInitialization,"transfers");
}

void ofApp::onFileReceivedCorrectly(ofxXMPPFileReceive & xmppfile){
	ofLogNotice() << "file received " << xmppfile.getMetadata().name << " with size " << xmppfile.getBuffer().size();
	ofFile file(xmppfile.getMetadata().name,ofFile::WriteOnly,true);
	file.writeFromBuffer(xmppfile.getBuffer());
}

void ofApp::onFileSavedCorrectly(ofxXMPPFileReceive & file){
	ofSystemAlertDialog(file.path() + "\nreceived correctly " );
}

//--------------------------------------------------------------
void ofApp::update(){
	sendButton.set(ofGetWidth()*.5-100,ofGetHeight()*.5-50,200,100);

}

//--------------------------------------------------------------
void ofApp::draw(){
	if(xmpp.getConnectionState()==ofxXMPPConnected){
		ofSetColor(ofColor::magenta);
		ofRect(sendButton);
		ofSetColor(ofColor::white);
		ofRectangle bb = font.getStringBoundingBox("Send!",0,0);
		font.drawString("Send!",sendButton.x+(sendButton.width - bb.width)*.5,sendButton.y+(sendButton.height-bb.height)*.5+bb.height);

		ofSetColor(ofColor::black);
		//ofDrawBitmapString(ofToString(xmppFileTransfer.getProgress()*100,2),20,20);

		vector<ofxXMPPUser> friends = xmpp.getFriends();

		if(selectedFriend>=0 && selectedFriend<friends.size()){
			if(friends[selectedFriend].chatState==ofxXMPPChatStateComposing){
				ofDrawBitmapString(friends[selectedFriend].userName + ": ...", 20, 20);
			}
		}

		for(int i=0;i<(int)friends.size();i++){
			ofSetColor(0);
			if(selectedFriend==i){
				ofSetColor(127);
				ofRect(ofGetWidth()-260,20+20*i-15,250,20);
				ofSetColor(255);
			}
			ofDrawBitmapString(friends[i].userName,ofGetWidth()-250,20+20*i);
			if(friends[i].show==ofxXMPPShowAvailable){
				ofSetColor(ofColor::green);
			}else{
				ofSetColor(ofColor::orange);
			}
			ofCircle(ofGetWidth()-270,20+20*i-5,3);
			//cout << friends[i].userName << endl;
			for(int j=0;j<friends[i].capabilities.size();j++){
				if(friends[i].capabilities[j]=="telekinect"){
					ofNoFill();
					ofCircle(ofGetWidth()-270,20+20*i-5,5);
					ofFill();
					break;
				}
			}
		}
	}else if(xmpp.getConnectionState()==ofxXMPPConnecting){
		ofSetColor(ofColor::black);
		ofDrawBitmapString("connecting...",ofGetWidth()/2-7*8,ofGetHeight()/2-4);
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key==OF_KEY_UP){
		selectedFriend--;
		selectedFriend %= xmpp.getFriends().size();
	}
	else if(key==OF_KEY_DOWN){
		selectedFriend++;
		selectedFriend %= xmpp.getFriends().size();
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
	ofVec2f mouse(x,y);
	if(xmpp.getConnectionState()==ofxXMPPConnected && sendButton.inside(mouse) && selectedFriend!=-1){
		ofFileDialogResult file = ofSystemLoadDialog("select a file to send",false);
		if(file.bSuccess){
			xmppFileTransfer.sendFile(xmpp.getFriends()[selectedFriend].userName + "/" + xmpp.getFriends()[selectedFriend].resource,file.getPath());
		}
	}else if(selectedFriend==-1){
		ofSystemAlertDialog("No user selected to send a file");
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

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
