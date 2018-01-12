# CropImage Introduction

Compile env: Ubuntu. I just test it on Ubuntu 16.04.

## **&#9733; Download code**

```language
sudo apt-get install git
git clone https://github.com/galian123/CropImage
```

## **&#9733; Configure Environment**

* Install opencv：
```language
sudo apt-get install libopencv-dev
```
You can also refer to https://docs.opencv.org/master/d7/d9f/tutorial_linux_install.html .

* Install cmake, boost, gflags:
    ```language
    sudo apt-get install cmake libboost-all-dev libgflags-dev
    ```

## **&#9733; Compile**

Current directory is `CropImage`.

```language
cd build
cmake ..
make
make install
```
The executable file `CropImage` will be generated in `./build` directory.
`make install` will put `CropImage` into your `~/bin`. You can add `~/bin` to your `$PATH` in `~/.bashrc`, like this: `PATH=~/bin:$PATH`. Then run `source ~/.bashrc`.

## **&#9733; Usage of `CropImage`**

Current directory is `CropImage`.
Use `CropImage -helpshort` to get more help information.

```language
./CropImage -helpshort
cropImage: crop images and save resize_width * resize_height area which contains labels.
It may split one image into several images with name suffix like 'xxx_1.png', 'xxx_2.png', etc.
Please use 'CropImage -helpshort' to get only help message for CropImage. 
Usage:
    CropImage -imgdir <image_folder> -labeldir <label_folder>
    CropImage -imgdir <image_folder> 
If '-labeldir' is not set, then use 'imgdir' as 'labeldir'.
Default image output folder is './output_image_<time>'. Default label output folder is './output_label_<time>'

  Flags from /home/galian/test/opencv/cropImage/cropImage.cpp:
    -imgdir (Set the input directory contains images.) type: string default: ""
    -labeldir (Set the input directory contains label files. If not set, use
      'imgdir' instead.) type: string default: ""
    -out_imgdir (Set the output directory which saves converted images.)
      type: string default: "./output_image"
    -out_labeldir (Set the output directory which saves converted label files.)
      type: string default: "./output_label"
    -resize_height (Height images are resized to) type: int32 default: 300
    -resize_width (Width images are resized to) type: int32 default: 300
    -verbose (show more logs) type: bool default: false
```

## **&#9733; Example**

Current directory is `CropImage`.

```language
./CropImage -imgdir ../test_image/
```

## **&#9733; Output**

Croped images will be saved to `output_image_<time>` folder, like this `output_image_300_300_2017-12-20_23-10-04`.

Annotation files (label files) are saved into `output_label_<time>` folder, like this `output_label_300_300_2017-12-20_23-10-04`.

Warning information will be saved into `result_warning_<time>.txt`, like this `result_warning_2017-12-20_23-10-04.txt`.

You can use [LabelImg](https://github.com/tzutalin/labelImg) to verify the result.

Original image file with annotations:
![](https://github.com/galian123/CropImage/blob/master/example/orig.png)

After image is cropped, it is divided into two parts.
![](https://github.com/galian123/CropImage/blob/master/example/crop1.png)

![](https://github.com/galian123/CropImage/blob/master/example/crop2.png)

## **&#9733; Notes**

### **&#9830; box boundary exceed resize_width \* resize_height**
If the annotation box boundary exceed resize_width \* resize_height (eg. 300 * 300), then warning info will be write to screen and log file.

print on screen: `this annotation box boundary exceed 300 * 300`

print to log file `./result_warning_<time>.txt`
```language
This annotation box boundary exceed 300 * 300, when processing ../test_image/99.jpg, labelfile: ../test_image/99.xml
(xmin: 470, xmax: 694, ymin: 88, ymax: 673, width: 225, height: 586)
```

### **&#9830; Now `<path>` and `<filename>` in annotation file are NOT changed after image is cropped.**




