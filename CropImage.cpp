#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string>
#include <opencv2/opencv.hpp>
#include "gflags/gflags.h"
#include <boost/foreach.hpp>
#include <boost/property_tree/xml_parser.hpp>

/*This tool is used to crop image into resize_width * resize_height( default: 300*300).
  And change the annotation file (xxx.xml) at the same time.
 */
using namespace boost::property_tree;
using namespace cv;
using namespace std;

#define DIR_NAME_LEN    256
#define FILE_NAME_LEN   256
#define LOG_LEN 1024
#define INF 10000000

DEFINE_string(imgdir, "", "Set the input directory contains images.");
DEFINE_string(labeldir, "",
    "Set the input directory contains label files. If not set, use 'imgdir' instead.");
DEFINE_string(out_imgdir, "./output_image",
    "Set the output directory which saves converted images.");
DEFINE_string(out_labeldir, "./output_label",
    "Set the output directory which saves converted label files.");
DEFINE_int32(resize_width, 300, "Width images are resized to");
DEFINE_int32(resize_height, 300, "Height images are resized to");
DEFINE_bool(verbose, false, "show more logs");

static bool verbose = false; 
static int resize_width = 0;
static int resize_height = 0;
static int fd_warning = 0;

class Box {
    public:
    Box(ptree obj, int xmin, int xmax, int ymin, int ymax);

    public:
        ptree obj;
        int xmin;
        int ymin;
        int xmax;
        int ymax;
};

Box::Box(ptree obj, int xmin, int xmax, int ymin, int ymax) {
    this->obj = obj;
    this->xmin = xmin;
    this->xmax = xmax;
    this->ymin = ymin;
    this->ymax = ymax;
}

static bool BoxCompare(const Box &b1, const Box &b2) {
    if (b1.xmin < b2.xmin) {
        return true;
    } else if (b1.xmin == b2.xmin && b1.ymin < b2.ymin) {
        return true;
    }
    return false;
}

int filter(const struct dirent *entry) {
    if (entry != NULL) {
    	if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
    	    return 0;
    	}   
        char *p = strrchr((char*)entry->d_name, '.');
        if (p != NULL &&  
            (strcasecmp(p, ".jpg") == 0 || strcasecmp(p, ".png") == 0
            || strcasecmp(p, ".jpeg") == 0)) {
            return 1;
        }
    }   
    return 0;
}

bool checkDirExist(char *dirName) {
    struct stat dirInfo = {0};
    int statRet = stat(dirName, &dirInfo);
    if (statRet == -1 && errno == ENOENT) {
        printf("%s not exist\n", dirName);
        return false; 
    } else {
        if ((dirInfo.st_mode & S_IFDIR) != S_IFDIR) {
            printf("%s is not directory.\n", dirName);
            return false; 
        }
    }   
    return true;
}

bool tryMkdir(char *dirName) {
    if (!checkDirExist(dirName)) {
        int ret = mkdir(dirName, 0775);
        if (ret != 0) {
            printf("mkdir %s failed, errno: %d, %s\n", dirName, errno, strerror(errno));
            return false;
        }
        printf("mkdir %s success\n", dirName);
    }
    return true;
}

void splitFileName(char *imgName, char outname[], char outext[]) {
    char *p = strrchr(imgName, '.');
    strncpy(outname, imgName, p - imgName);
    strcpy(outext, p);
}

void getLabelFileName(char *imgName, char outname[]) {
    char *p = strrchr(imgName, '.');
    char temp[256] = {0};
    strncpy(temp, imgName, p - imgName);
    sprintf(outname, "%s%s", temp, ".xml");
}

void tryAddSlash(char dir[]) {
    int len = 0;
    len = strlen(dir);
    if (dir[len-1] != '/') {
        dir[len] = '/';
        dir[len+1] = 0;
    }
}

bool writeLabelFileForPartialBoxes(char *origlabelfile, char *outfilename, vector<Box> boxes, int xlimit, int ylimit) {
    ptree pt;
    read_xml(origlabelfile, pt, xml_parser::trim_whitespace);
    try {
        pt.put<int>("annotation.size.height", resize_height);
        pt.put<int>("annotation.size.width", resize_width);

        ptree& objects = pt.get_child("annotation");
        for (ptree::iterator it = objects.begin(); it != objects.end();) {
            if (it->first == "object") {
                it = objects.erase(it);
            } else {
                ++it;
            }
        }

        vector<ptree> objvector;
        for (vector<Box>::iterator it = boxes.begin(); it != boxes.end(); it++) {
            ptree obj = it->obj;
            assert(it->xmin >= xlimit);
            obj.put<int>("bndbox.xmin", it->xmin - xlimit);
            assert(it->xmax >= xlimit);
            obj.put<int>("bndbox.xmax", it->xmax - xlimit);
            assert(it->ymin >= ylimit);
            obj.put<int>("bndbox.ymin", it->ymin - ylimit);
            assert(it->ymax >= ylimit);
            obj.put<int>("bndbox.ymax", it->ymax - ylimit);
            objvector.push_back(obj);
        }
        for (int i = 0; i < objvector.size(); i++) {
            ptree obj = objvector[i];
            pt.add_child("annotation.object", obj);
        }
    } catch (const ptree_error &e) {
        printf("error happens: %s\n", e.what());
        return false;
    }

    boost::property_tree::xml_writer_settings<string> settings('\t',1);
    write_xml(outfilename, pt, std::locale(), settings);
    return true;
}

bool writeLabelFile(char *origlabelfile, char *outfilename, int xlimit, int ylimit) {
    ptree pt;
    read_xml(origlabelfile, pt, xml_parser::trim_whitespace);
    try {
        pt.put<int>("annotation.size.height", resize_height);
        pt.put<int>("annotation.size.width", resize_width);

        vector<ptree> objvector;
        ptree& objects = pt.get_child("annotation");
        for (ptree::iterator it = objects.begin(); it != objects.end();) {
            if (it->first == "object") {
                ptree obj = it->second;
                int temp = obj.get<int>("bndbox.xmin");
                assert(temp >= xlimit);
                obj.put<int>("bndbox.xmin", temp - xlimit);
                temp = obj.get<int>("bndbox.xmax");
                assert(temp >= xlimit);
                obj.put<int>("bndbox.xmax", temp - xlimit);
                temp = obj.get<int>("bndbox.ymin");
                assert(temp >= ylimit);
                obj.put<int>("bndbox.ymin", temp - ylimit);
                temp = obj.get<int>("bndbox.ymax");
                assert(temp >= ylimit);
                obj.put<int>("bndbox.ymax", temp - ylimit);
                objvector.push_back(obj);
                it = objects.erase(it);
            } else {
                ++it;
            }
        }
        for (int i = 0; i < objvector.size(); i++) {
            ptree obj = objvector[i];
            pt.add_child("annotation.object", obj);
        }
    } catch (const ptree_error &e) {
        printf("error happens: %s\n", e.what());
        return false;
    }

    boost::property_tree::xml_writer_settings<string> settings('\t',1);
    write_xml(outfilename, pt, std::locale(), settings);
    return true;
}

int main(int argc, char** argv ) {
    gflags::SetUsageMessage("crop images and save resize_width * resize_height area which contains labels.\n"
        "It may split one image into several images with name suffix like 'xxx_1.png', 'xxx_2.png', etc.\n"
        "Please use 'cropImage -helpshort' to get only help message for cropImage. \n"
        "Usage:\n"
        "    cropImage -imgdir <image_folder> -labeldir <label_folder> \n"
        "    cropImage -imgdir <image_folder> \n"
        "If '-labeldir' is not set, then use 'imgdir' as 'labeldir'.\n"
        "Default image output folder is './output_img_<time>'. Default label output folder is './output_label_<time>'\n");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    const string imgdir = FLAGS_imgdir;
    string labeldir = FLAGS_labeldir;
    const string out_imgdir = FLAGS_out_imgdir;
    const string out_labeldir = FLAGS_out_labeldir;
    resize_width = FLAGS_resize_width;
    resize_height = FLAGS_resize_height;
    verbose = FLAGS_verbose;

    if (imgdir.empty()) {
        printf("imgdir should not be empty, use -helpshort to get more help info.\n");
        return -1;
    }
    if (labeldir.empty()) {
        printf("use %s as labeldir\n", imgdir.c_str());
        labeldir = imgdir;
    }
	
    if (verbose) printf("image path: %s\n", imgdir.c_str());
    if (verbose) printf("label path: %s\n", labeldir.c_str());
    if (verbose) printf("output image path: %s\n", out_imgdir.c_str());
    if (verbose) printf("output label path: %s\n", out_labeldir.c_str());
    if (verbose) printf("resize_width: %d, resize_height: %d\n", resize_width, resize_height);

   	time_t t = time(0);
  	struct tm *tmp = localtime(&t);
  	char time_str[64] = {0};
  	strftime(time_str, sizeof(time_str), "%F_%H-%M-%S", tmp);
   
    char dirName[DIR_NAME_LEN] = {0};
    strcpy(dirName, imgdir.c_str());
    dirName[DIR_NAME_LEN - 1] = '\0';
    if (!checkDirExist(dirName)) {
        printf("%s doesn't exist.\n", dirName);
        return -1;
    }

    char outImgDir[DIR_NAME_LEN + 1] = {0};
    sprintf(outImgDir, "%s_%d_%d_%s", out_imgdir.c_str(), resize_width, resize_height, time_str);
    if (!tryMkdir(outImgDir)){
        return -1;
    }

    char outLabelDir[DIR_NAME_LEN + 1] = {0};
    sprintf(outLabelDir, "%s_%d_%d_%s", out_labeldir.c_str(), resize_width, resize_height, time_str);
    if (!tryMkdir(outLabelDir)){
        return -1;
    }

    char labelDir[DIR_NAME_LEN + 1] = {0};
    strcpy(labelDir, labeldir.c_str());

    tryAddSlash(dirName);
    tryAddSlash(labelDir);
    tryAddSlash(outImgDir);
    tryAddSlash(outLabelDir);

    struct dirent **namelist;
    int n = scandir(dirName, &namelist, filter, alphasort);
    if (n <= 0) {
        printf("Error: no files in %s\n", argv[1]);
        return -1;
    }

    char warning_log[FILE_NAME_LEN] = {0};
    sprintf(warning_log, "%s%s%s", "./result_warning_", time_str, ".txt");
    fd_warning = open(warning_log, O_CREAT | O_RDWR, 0666);
    bool hasWarning = false;

    int cnt = n; 
    for (int i = 0; i < n; i++) {
        printf("--------------------\n");
        // image name
        char *pname = namelist[i]->d_name;
        printf("file name: %s\n", pname);
        char fullname[FILE_NAME_LEN] = {0};
        sprintf(fullname, "%s%s", dirName, pname);

		// process label file
        char labelfile[FILE_NAME_LEN] = {0};
        char lname[FILE_NAME_LEN] = {0};
        getLabelFileName(pname, lname);
        sprintf(labelfile, "%s%s", labelDir, lname);
        printf("label file name: %s\n", labelfile);

    	ptree pt;
  		read_xml(labelfile, pt, xml_parser::trim_whitespace);
        int width = 0, height = 0;
        try {
            height = pt.get<int>("annotation.size.height");
            width = pt.get<int>("annotation.size.width");
        } catch (const ptree_error &e) {
            char str[LOG_LEN] = {0};
            sprintf(str, "When parsing %s, error happens: %s. Ignore %s\n\n", labelfile, e.what(), labelfile);
            str[1023] = 0;
            printf("%s", str);
            write(fd_warning, str, strlen(str));
            hasWarning = true;
			continue;
        }

        if (verbose) printf("width: %d, height: %d\n", width, height);
        if (height < resize_height || width < resize_width) {
            printf("Height %d, or width %d is smaller than resize_height(%d) or resize_width(%d).\n",
                height, width, resize_height, resize_width);
            char str[LOG_LEN] = {0};
            sprintf(str, "Height %d, or width %d is smaller than resize_height(%d) or resize_width(%d). Ignore %s, labelfile: %s\n\n", 
                height, width, resize_height, resize_width, fullname, labelfile);
            str[1023] = 0;
            write(fd_warning, str, strlen(str));
            hasWarning = true;
            continue;
        }

        vector<Box> boxes;
        int xmin = INF;
        int ymin = INF;
        int xmax = 0, ymax = 0;
        try {
            BOOST_FOREACH(ptree::value_type &v1, pt.get_child("annotation")) {
                if (v1.first == "object") {
                    ptree obj = v1.second;
                    int x1 = obj.get<int>("bndbox.xmin");
                    if (xmin > x1) {
                        xmin = x1;
                    }
                    int x2 = obj.get<int>("bndbox.xmax");
                    if (xmax < x2) {
                        xmax = x2;
                    }
                    int y1 = obj.get<int>("bndbox.ymin");
                    if (ymin > y1) {
                        ymin = y1;
                    }
                    int y2 = obj.get<int>("bndbox.ymax");
                    if (ymax < y2) {
                        ymax = y2;
                    }
                    Box box(obj, x1, x2, y1, y2);
                    boxes.push_back(box);
                }
            }
        } catch (const ptree_error &e) {
            printf("error happens: %s\n", e.what());
            char str[LOG_LEN] = {0};
            sprintf(str, "Error happens: %s when processing %s, labelfile: %s\n\n", 
                e.what(), fullname, labelfile);
            str[1023] = 0;
            write(fd_warning, str, strlen(str));
            hasWarning = true;
            continue;
        }

        if (verbose) printf("%ld traffic lights\n", boxes.size());
        if (verbose) printf("xmin: %d, xmax: %d, ymin: %d, ymax: %d\n", xmin, xmax, ymin, ymax);
        int w = xmax - xmin + 1;
        int h = ymax - ymin + 1;
        if (w >= resize_width || h >= resize_height) {// should split into multiple images
            char name[FILE_NAME_LEN] = {0};
            char ext[32] = {0};
            splitFileName(pname, name, ext);
            //if (verbose) printf("pname: %s, name: %s, ext: %s\n", pname, name, ext);
            int idSuffix = 1;// splited images are renamed to xxx_1.png, xxx_2.png, label files will be xxx_1.xml, xxx_2.xml
            int cnt = boxes.size();
            // sort boxes
            partial_sort(boxes.begin(), boxes.begin()+cnt, boxes.end(), BoxCompare);

            while (cnt > 0) {
                vector<Box> tempBoxes;
                tempBoxes.clear();
                Box b1 = boxes[0];
                int x1 = b1.xmin;
                int x2 = b1.xmax;
                int y1 = b1.ymin;
                int y2 = b1.ymax;
                if (verbose) printf("b1.xmin: %d, b1.xmax: %d, b1.ymin: %d, b1.ymax: %d\n", x1, x2, y1, y2); 
                tempBoxes.push_back(b1);
                vector<int> delIdx;
                delIdx.push_back(0);
                for (int i = 1; i < boxes.size(); i++) {
                    Box b2 = boxes[i];
                    if (abs(b2.xmax - x1) < resize_width && abs(b2.ymax - y1) < resize_height
                        && abs(x2 - b2.xmin) < resize_width && abs(y2 - b2.ymin) < resize_height) {
                        if (verbose) printf("b2.xmin: %d, b2.xmax: %d, b2.ymin: %d, b2.ymax: %d\n",
                            b2.xmin, b2.xmax, b2.ymin, b2.ymax); 
                        tempBoxes.push_back(b2);
                        delIdx.push_back(i);
                        if (x1 > b2.xmin) x1 = b2.xmin;
                        if (x2 < b2.xmax) x2 = b2.xmax;
                        if (y1 > b2.ymin) y1 = b2.ymin;
                        if (y2 < b2.ymax) y2 = b2.ymax;
                    }
                }
                cnt -= delIdx.size();
                for (int i = delIdx.size() - 1; i >= 0; i--) {
                    boxes.erase(boxes.begin() + delIdx[i]);
                }
                if (verbose) printf("boxes size: %ld\n", boxes.size());
                
                w = x2 - x1 + 1;
                h = y2 - y1 + 1;
                if (resize_width < w || resize_height < h) {
                    printf("this annotation box boundary exceed %d * %d\n", resize_width, resize_height);
                    char str[LOG_LEN] = {0};
                    sprintf(str, "This annotation box boundary exceed %d * %d, when processing %s, labelfile: %s\n",
                        resize_width, resize_height, fullname, labelfile);
                    write(fd_warning, str, strlen(str));
                    memset(str, 0, sizeof(str));
                    sprintf(str, "(xmin: %d, xmax: %d, ymin: %d, ymax: %d, width: %d, height: %d)\n\n", 
                        x1, x2, y1, y2, w, h);
                    write(fd_warning, str, strlen(str));
                    hasWarning = true;
                    continue;
                }
            	int w2 = (resize_width - w)/2;
            	int h2 = (resize_height - h)/2;
            	int xlimit = x1 - w2;
            	if (xlimit < 0) xlimit = 0;
            	int ylimit = y1 - h2;
            	if (ylimit < 0) ylimit = 0;

        		if (xlimit + resize_width > width) {
        		    xlimit = width - resize_width;
        		}
        		if (ylimit + resize_height > height) {
        		    ylimit = height - resize_height;
        		}

            	if (verbose) printf("xlimit: %d, ylimit: %d\n", xlimit, ylimit);
                char outlabelfile[FILE_NAME_LEN] = {0};
                sprintf(outlabelfile, "%s%s_%d.xml", outLabelDir, name, idSuffix);
                printf("outlabelfile: %s\n", outlabelfile);
            	writeLabelFileForPartialBoxes(labelfile, outlabelfile, tempBoxes, xlimit, ylimit);

            	try {
            	    Mat src = imread(fullname, -1);
            	    Mat dst(src, Rect(xlimit, ylimit, resize_width, resize_height));
            	    char outname[FILE_NAME_LEN] = {0};
            	    sprintf(outname, "%s%s_%d%s", outImgDir, name, idSuffix, ext);
            	    printf("output image: %s\n", outname);
            	    imwrite(outname, dst);
            	} catch (runtime_error &e) {
            	    printf("error: %s\n", e.what());
                    char str[LOG_LEN] = {0};
                    sprintf(str, "Error happens %s when processing %s, labelfile: %s\n", e.what(), 
                        fullname, labelfile);
                    write(fd_warning, str, strlen(str));
                    hasWarning = true;
            	}
                idSuffix++;
            }
        } else {
            int w2 = (resize_width - w)/2;
            int h2 = (resize_height - h)/2;
            int xlimit = xmin - w2;
            if (xlimit < 0) xlimit = 0;
            int ylimit = ymin - h2;
            if (ylimit < 0) ylimit = 0;

        	if (xlimit + resize_width > width) {
        	    xlimit = width - resize_width;
        	}
        	if (ylimit + resize_height > height) {
        	    ylimit = height - resize_height;
        	}

            char outlabelfile[FILE_NAME_LEN] = {0};
            sprintf(outlabelfile, "%s%s", outLabelDir, lname);
            printf("output label file: %s\n", outlabelfile);

            if (verbose) printf("xlimit: %d, ylimit: %d\n", xlimit, ylimit);
            writeLabelFile(labelfile, outlabelfile, xlimit, ylimit);

            try {
                Mat src = imread(fullname, -1);
                Mat dst(src, Rect(xlimit, ylimit, resize_width, resize_height));
                char outname[FILE_NAME_LEN] = {0};
                sprintf(outname, "%s%s", outImgDir, pname);
                printf("output image: %s\n", outname);
                imwrite(outname, dst);
            } catch (runtime_error &e) {
                printf("error: %s\n", e.what());
                char str[LOG_LEN] = {0};
                sprintf(str, "Error happens %s when processing %s, labelfile: %s\n\n", e.what(), 
                    fullname, labelfile);
                write(fd_warning, str, strlen(str));
                hasWarning = true;
            }
        }
    }
    printf("--------------------\n");
    if (hasWarning) {
        printf("Please check warning log '%s' for more information.\n", warning_log);
    } else {
        printf("Everything is OK. Congratulations to you.\n");
    }
    while (cnt--) {
        free(namelist[cnt]);
    }
    free(namelist);
	close(fd_warning);
    return 0;
}
