import torch
from torch import nn
from torch.utils.data import Dataset
from torch.utils.data import DataLoader
from torchvision import datasets
from torchvision.transforms import ToTensor
import os
import pandas as pd
import PIL
import numpy as np

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
        label = self.img_labels.iloc[idx, 1]
        return image[0], torch.FloatTensor([float(label)])

# Inside the training and test folders are subfolders, each contains a stack of 33 33*33 pixel images, loaded in alphabetical order
training_data = CustomDataset("../inductwind/training.csv","../inductwind/training")
test_data = CustomDataset("../inductwind/test.csv","../inductwind/test")
		
print("training_data len is %d"%len(training_data))
batch_size = 8

# Create data loaders.
train_dataloader = DataLoader(training_data, batch_size=batch_size, shuffle=True)
test_dataloader = DataLoader(test_data, batch_size=batch_size, shuffle=True)

for X, y in test_dataloader:
    print(f"Shape of X [N, C, H, W]: {X.shape}")
    print(f"Shape of y: {y.shape} {y.dtype}")
    
	
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
class NeuralNetwork(nn.Module):
    def __init__(self):
        super().__init__()
        self.flatten = nn.Flatten()
        self.linear_relu_stack = nn.Sequential(
            nn.Linear(33*33*33, 512),
            nn.ReLU(),
            nn.Linear(512, 512),
            nn.ReLU(),
            nn.Linear(512, 1)
        )

    def forward(self, x):
        x = self.flatten(x)
        logits = self.linear_relu_stack(x)
        return logits

model = NeuralNetwork().to(device)
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

        print("When training, input shape is:")
        print(X.shape)
		
        # Compute prediction error
        pred = model(X)
        loss = loss_fn(pred, y)

        #print(pred)
        #print(y)
		
        # Backpropagation
        loss.backward()
        optimizer.step()
        optimizer.zero_grad()

        if batch % 100 == 0:
            loss, current = loss.item(), (batch + 1) * len(X)
            print(f"loss: {loss:>7f}  [{current:>5d}/{size:>5d}]")
			
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
	
epochs = 2
for t in range(epochs):
    print(f"Epoch {t+1}\n-------------------------------")
    train(train_dataloader, model, loss_fn, optimizer)
    test(train_dataloader, model, loss_fn)
print("Done!")

# Load a stack of 33 tifs
imageStack = torch.zeros([1,33,1089],dtype=torch.float32)

for i in range(0,33):
    img = PIL.Image.open("../construct/02512_03988_02012/%05d.tif" % (i+2012))
    img = PIL.Image.fromarray(np.uint8(np.array(img)/256))
    imageStack[0,:,i:i+33]= (ToTensor()(img))[0,0:33,0:33]
	
print(imageStack.shape)
imageStack = imageStack.to(device="cuda")
print(model(imageStack))

# TODO - check that training and applied images are both constructed in the same way
# run it over a whole cube