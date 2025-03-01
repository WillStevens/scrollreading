import datetime
import torch
from torch import nn
import torch.nn.functional as F
from torch.utils.data import Dataset
from torch.utils.data import DataLoader
from torchvision import datasets
from torchvision.transforms import ToTensor
import os
import pandas as pd
import PIL
import numpy as np

imageSize = 65

class CustomDataset(Dataset):
    def __init__(self, annotations_file, img_dir):
        self.img_labels = pd.read_csv(annotations_file)
        self.img_dir = img_dir
        print(self.img_labels)

    def __len__(self):
        return len(self.img_labels)

    def __getitem__(self, idx):
        img_path = os.path.join(self.img_dir, self.img_labels.iloc[idx, 0])
        image = ToTensor()(PIL.Image.open(img_path).convert('L'))
        # Convert a N*N by N tensor into a N by N by N tensor
        image3D = torch.empty(1,imageSize,imageSize,imageSize)
        for i in range(0,imageSize):
          image3D[0][i] = image[0,:,i*imageSize:i*imageSize+imageSize]
        label = self.img_labels.iloc[idx, 1]
        return image3D, torch.FloatTensor([float(label)])

training_data = CustomDataset("../inductwind/training65.csv","../inductwind/training65")
#test_data = CustomDataset("../inductwind/test.csv","../inductwind/test")
        
print("training_data len is %d"%len(training_data))
batch_size = 8

# Create data loaders.
train_dataloader = DataLoader(training_data, batch_size=batch_size, shuffle=True)
#test_dataloader = DataLoader(test_data, batch_size=batch_size, shuffle=True)

#for X, y in train_dataloader:
#    print(f"Shape of X [N, C, H, W]: {X.shape}")
#    print(f"Shape of y: {y.shape} {y.dtype}")
    
    
# Get cpu, gpu or mps device for training.
device = (
    "cuda"
    if torch.cuda.is_available()
    else "mps"
    if torch.backends.mps.is_available()
    else "cpu"
)
print(f"Using {device} device")

# Define model
class Conv3DNet(nn.Module):
    def __init__(self):
        super(Conv3DNet, self).__init__()
        self.conv1 = nn.Conv3d(1, 6, 3)  # Input: 1 x 33 x 33 x 33, output 6 x 31 x 31 x 31
        self.pool = nn.MaxPool3d(2) # Input: 6 x 31 x 31 x 31, output 6 x 15 x 15 x 15
        self.conv2 = nn.Conv3d(6, 16, 5) # Input 6 x 15 x 15 x 15, output 16 x 11 x 11 x 11
                                         # use self.pool again: Input: 16 x 11 x 11 x 11, output 16 x 5 x 5 x 5
        self.fc1 = nn.Linear(35152, 256)
        self.fc2 = nn.Linear(256, 64)
        self.fc3 = nn.Linear(64, 1)

    def forward(self, x):
        x = self.pool(F.relu(self.conv1(x)))
        x = self.pool(F.relu(self.conv2(x)))
        x = x.view(-1, 35152)
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = self.fc3(x)
        return x

class NeuralNetwork(nn.Module):
    def __init__(self):
        super().__init__()
        self.flatten = nn.Flatten()
        self.linear_relu_stack = nn.Sequential(
            nn.Linear(33*33*33, 128),
            nn.ReLU(),
            nn.Linear(128, 32),
            nn.ReLU(),
            nn.Linear(32, 1)
        )

    def forward(self, x):
        x = self.flatten(x)
        logits = self.linear_relu_stack(x)
        return logits

model = Conv3DNet().to(device)
#model = NeuralNetwork().to(device)
print(model)

loss_fn = nn.L1Loss()
optimizer = torch.optim.SGD(model.parameters(), lr=1e-3)

def train(dataloader, model, loss_fn, optimizer):
    size = len(dataloader.dataset)
    print("size=%d"%size)
    model.train()
    i=0
    for batch, (X, y) in enumerate(dataloader):
        #print("training %d"%i)
        i+=1
        X, y = X.to(device), y.to(device)

        #print("When training, input shape is:")
        #print(X.shape)
        
        # Compute prediction error
        pred = model(X)
        loss = loss_fn(pred, y)

        #print(pred)
        #print(y)
        
        # Backpropagation
        loss.backward()
        optimizer.step()
        optimizer.zero_grad()

        #if batch % 100 == 0:
        #    loss, current = loss.item(), (batch + 1) * len(X)
        #    print(f"loss: {loss:>7f}  [{current:>5d}/{size:>5d}]")
            
def test(dataloader, model, loss_fn):
    size = len(dataloader.dataset)
    num_batches = len(dataloader)
    model.eval()
    test_loss, correct = 0, 0
    with torch.no_grad():
        for X, y in dataloader:
            X, y = X.to(device), y.to(device)
            pred = model(X)
            test_loss += loss_fn(pred, y).item()
            correct += (pred.argmax(1) == y).type(torch.float).sum().item()
    test_loss /= num_batches
    correct /= size
    print(f"Test Error: \n Accuracy: {(100*correct):>0.1f}%, Avg loss: {test_loss:>8f} \n")
    
epochs = 100
for t in range(epochs):
    print(f"Epoch {t+1}\n-------------------------------")
    print("Start: " + str(datetime.datetime.now()))
    train(train_dataloader, model, loss_fn, optimizer)
#    test(train_dataloader, model, loss_fn)
    print("End: " + str(datetime.datetime.now()))
print("Done!")

images = []
for i in range(0,imageSize):
    imgTmp = PIL.Image.open("../construct/02512_03988_02012/%05d.tif" % (i+2012))
    images += [ToTensor()(PIL.Image.fromarray(np.uint8(np.array(imgTmp)/256))).to(device="cuda")]

debug = True
(outx,outy)=(432,432)

step = 16

inputTensor3D = torch.empty(step,1,imageSize,imageSize,imageSize).to(device="cuda")

print(images[0].shape)
print(inputTensor3D.shape)

outImageArray = np.empty((outy,outx),dtype=np.uint8)
for y in range(0,outy,step):
    for x in range(0,outx):
        if debug:
            print("Before tensor init " + str(datetime.datetime.now()))
        if debug:
            print("Before tensor populate " + str(datetime.datetime.now()))
        for z in range(0,imageSize):
            for s in range(0,step):
                inputTensor3D[s][0][z] = images[z][0,y+s:y+imageSize+s,x:x+imageSize]
        if debug:
            print("Before tensor target " + str(datetime.datetime.now()))
    
        #inputTensor = inputTensor.to(device="cuda")
        
        if debug:
            print("Before NN " + str(datetime.datetime.now()))
        output = model(inputTensor3D)
        if debug:
            print("After NN " + str(datetime.datetime.now()))
        for s in range(0,step):
            o = output[s].item()
            if o<0.0:
               o = 0.0
            if o>1.0:
               o = 1.0
            outImageArray[y+s,x]=o*255
    print(str(y) + " " + str(datetime.datetime.now()))

PIL.Image.fromarray(outImageArray).save("testout.png")
    
# TODO - check that training and applied images are both constructed in the same way
# run it over a whole cube