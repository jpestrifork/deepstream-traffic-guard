#include <string>
#include <string.h>
#include <iostream>
#include <vector>
#include <locale>
#include "nvdsinfer.h"
#include <fstream>
#include "nvdsinfer_custom_impl.h"

using namespace std;
using std::string;
using std::vector;

static const string kPathToCharSet = "/workspace/models/lpr_us/us_lp_characters.txt";
static const int kMaxPlateLength = 16;
static bool dict_ready = false;
static std::vector<string> dict_table;

void ReadLprUsCharacters(){
    ifstream fdict;
    setlocale(LC_CTYPE, "");

    fdict.open(kPathToCharSet);
    if (!fdict.is_open()) {
        cout << "open dictionary file failed." << endl;
        return; 
    }

    while (!fdict.eof()) {
        string strLineAnsi;
        if (getline(fdict, strLineAnsi)) {
            dict_table.push_back(strLineAnsi);
        }
    }
    fdict.close();

    dict_ready = true;
}

void GetBufferArrays(const std::vector<NvDsInferLayerInfo> &outputLayersInfo, 
                    float* &outputConfBuffer, 
                    int* &outputChrBuffer)
{
    for (unsigned int li = 0; li < outputLayersInfo.size(); li++) {
        if (outputLayersInfo[li].isInput)
            continue;
        
        if (outputLayersInfo[li].dataType == NvDsInferDataType::FLOAT) {
            if (!outputConfBuffer)
                outputConfBuffer = static_cast<float *>(outputLayersInfo[li].buffer);
        } else if (outputLayersInfo[li].dataType == NvDsInferDataType::INT32) {
            if (!outputChrBuffer)
                outputChrBuffer = static_cast<int *>(outputLayersInfo[li].buffer);
        }
    }
}

vector<int> CtcDecoder(int seq_len, 
                const float* outputConfBuffer,
                const int* outputChrBuffer,
                std::vector<double> &probs,
                unsigned int &plate_char_count)
{

    bool do_softmax = false;
    int prev;
    vector<int> idx;

    for (int seq_id = 0; seq_id < seq_len; seq_id++) {

        int curr_data = outputChrBuffer[seq_id];
        if (curr_data < 0 || curr_data > static_cast<int>(dict_table.size()))
            continue;

        if (seq_id == 0) {
            prev = curr_data;
            idx.push_back(curr_data);
            if (curr_data != static_cast<int>(dict_table.size())) 
                do_softmax = true;
        } else {
            if (curr_data != prev) {
                idx.push_back(curr_data);
                if (curr_data != static_cast<int>(dict_table.size()))
                    do_softmax = true;
            }
            prev = curr_data;
        }

        // Do softmax
        if (do_softmax) {
            do_softmax = false;
            probs[plate_char_count] = outputConfBuffer[seq_id];
            plate_char_count++;
        }
    }

    return idx;
}

string GetPlate(const std::vector<int> &idx){
    string plate = "";
    for(unsigned int id = 0; id < idx.size(); id++) {
        if (static_cast<unsigned int>(idx[id]) != dict_table.size())
            plate += dict_table[idx[id]];
    }
    return plate;
}

extern "C"
bool NvDsInferParseCustomNVPlate(std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
                                 NvDsInferNetworkInfo const &networkInfo, float classifierThreshold,
                                 std::vector<NvDsInferAttribute> &attrList, std::string &attrString)
{   
    int *outputChrBuffer = NULL;
    float *outputConfBuffer = NULL;

    NvDsInferAttribute LPR_attr;
    LPR_attr.attributeConfidence = 1.0;

    int seq_len = networkInfo.width/4; 
    vector<double> probs(kMaxPlateLength, 0.0); 
    unsigned int plate_char_count = 0;

    if(!dict_ready) {
        ReadLprUsCharacters();
    }
    
    GetBufferArrays(outputLayersInfo, outputConfBuffer, outputChrBuffer);
    vector<int> idx = CtcDecoder(seq_len, outputConfBuffer, outputChrBuffer, probs, plate_char_count);
    attrString = GetPlate(idx);

    //Ignore the short string, it may be wrong plate string
    if (plate_char_count >=  3) {
        LPR_attr.attributeIndex = 0;
        LPR_attr.attributeValue = 1;
        LPR_attr.attributeLabel = strdup(attrString.c_str());
        for (unsigned int count = 0; count < plate_char_count; count++) {
            LPR_attr.attributeConfidence *= probs[count];
        }
        attrList.push_back(LPR_attr);
    }

    return true;
}

CHECK_CUSTOM_CLASSIFIER_PARSE_FUNC_PROTOTYPE(NvDsInferParseCustomNVPlate);