import pimp
import os
from PyQt4 import QtGui

class InterceptWindow(QtGui.QWidget):
    
    def __init__(self, title, data):
        super(InterceptWindow, self).__init__()
        self.title = title
        self.data = data
        self.setWindowProperty()
        self.setWindowLayout()
        self.setConnections()
        self.show()

    def setWindowProperty(self):
        self.setGeometry(1500, 500, 500, 248)
        self.setWindowTitle(self.title)
        
    def setWindowLayout(self):
        self.bounceButton = QtGui.QPushButton("Bounce")
        self.saveButton   = QtGui.QPushButton("Save")
        self.cancelButton = QtGui.QPushButton("Cancel")
        self.editField = QtGui.QTextEdit()
        self.editField.insertPlainText(self.data)
        hbox = QtGui.QHBoxLayout()
        hbox.addStretch(1)
        hbox.addWidget(self.saveButton)
        hbox.addWidget(self.cancelButton)
        hbox.addWidget(self.bounceButton)
        vbox = QtGui.QVBoxLayout()
        vbox.addStretch(1)
        vbox.addWidget(self.editField)
        vbox.addLayout(hbox)
        self.setLayout(vbox)
        
    def setConnections(self):
        self.bounceButton.clicked.connect(self.updateText)
        self.cancelButton.clicked.connect(QtGui.QApplication.quit)
        self.saveButton.clicked.connect(self.writeFile)
    
    def writeFile(self):
        filename = QtGui.QFileDialog().getOpenFileName(self,"Save As",os.getenv("HOME"))
        if len(filename) == 0:
            return
        with open(filename, "w") as f:
            f.write(self.data)
            
    def updateText(self):
        self.data = self.editField.toPlainText()
        QtGui.QApplication.quit()

def intercept(rid, text, title):
    app = QtGui.QApplication([""])
    win = InterceptWindow("%s %d" % (title, rid), text)
    app.exec_()
    return str(win.data)
    
def proxenet_request_hook(request_id, request):
    data = intercept(request_id, request.replace(pimp.CRLF, "\x0a"), "Intercepting request")
    data = data.replace("\x0a", pimp.CRLF)
    return data

def proxenet_response_hook(response_id, response):
    # data = intercept(response_id, response.replace(pimp.CRLF, "\x0a"), "Intercepting response")
    # data = data.replace("\x0a", pimp.CRLF)
    data = response
    return data

    
if __name__ == "__main__":
    req = "GET / HTTP/1.1\r\nHost: foo\r\nX-Header: Powered by proxenet\r\n\r\n"
    rid = 10
    
    print proxenet_request_hook(rid, req)

