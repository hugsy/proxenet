import pimp
import sys
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
        self.setGeometry(300, 300, 290, 150)
        self.setWindowTitle(self.title)

        
    def setWindowLayout(self):      
        self.bounceButton = QtGui.QPushButton("Bounce")
        self.saveButton   = QtGui.QPushButton("Save")
        self.cancelButton = QtGui.QPushButton("Cancel")
        self.editField = QtGui.QTextEdit(self.data)
        
        hbox = QtGui.QHBoxLayout()
        hbox.addStretch(1)
        hbox.addWidget(self.bounceButton)
        hbox.addWidget(self.cancelButton)

        vbox = QtGui.QVBoxLayout()
        vbox.addStretch(1)
        vbox.addWidget(self.editField)
        vbox.addLayout(hbox)
        
        self.setLayout(vbox)
        
        
    def setConnections(self):
        # self.cancelButton.clicked.connect(self.quit)
        self.saveButton.clicked.connect(self.write_in_file)

    
    def write_in_file(self):
        fd = QtGui.QFileDialog(self)
        filename = fd.getOpenFileName()
        with open(filename, "w") as f:
            f.write(self.data)
            
    
def proxenet_request_hook(id, request):
    return request

def proxenet_response_hook(id, response):
    return reponse

    
if __name__ == "__main__":
    req = "GET   /   HTTP/1.1\r\nHost: foo\r\nX-Header: Powered by proxenet\r\n\r\n"
    app = QtGui.QApplication(sys.argv)
    win = InterceptWindow("Intercepting request %d" % 10, req)
    sys.exit(app.exec_())
    

