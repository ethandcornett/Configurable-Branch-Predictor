#include <vector>
#include <iostream>
#include <cmath>
#include <iomanip>
#ifndef SIM_BP_H
#define SIM_BP_H

typedef struct bp_params{
    unsigned long int K;    // K is the number of PC bits used to index the chooser table
    unsigned long int M1;   // M1 is the number of PC bits used to index the gshare table
    unsigned long int M2;   // M2 is the number of PC bits used to index the bimodal table
    unsigned long int N;    // N is the number of global history register bits used to index the gshare table
    char*             bp_name;
}bp_params;

// Put additional data structures here as per your requirement

class BranchHistoryTable{
public:
    // Vector of ints to store the history of the branch predictor
    std::vector<int> bimodal_table;     // Branch history counter goes from 0 to 3 with 0 being strongly not taken and 3 being strongly taken
    std::vector<int> gshare_table;      // Branch history counter goes from 0 to 3 with 0 being strongly not taken and 3 being strongly taken
    std::vector<int> hybrid_table;     // Branch history counter goes from 0 to 3 with 0 being strongly not taken and 3 being strongly taken

    int bimodal_index_size;     // Size of the index for the bimodal predictor
    int gshare_index_size;     // Size of the index for the gshare predictor
    int hybrid_index_size;     // Size of the index for the hybrid predictor

    int global_history;     // Global history register - used for gshare predictor

    bp_params bp_param; // store the parameters for the current branch predictor

    // Measurement counters
    int number_of_predictions;  // number of dynamic branches in the trace
    int number_of_mispredictions; // predicted taken when not-taken, or predicted not-taken when taken


    BranchHistoryTable(bp_params bp_param);
    void predict_bimodal_branch(int addr, char outcome);
    void predict_gshare_branch(int addr, char outcome);
    void predict_hybrid_branch(int addr, char outcome);

    void print_contents();
};

BranchHistoryTable::BranchHistoryTable(bp_params bp_param){
    this->bp_param = bp_param;

    // Initialize measurement counters
    this->number_of_predictions = 0;
    this->number_of_mispredictions = 0;

    // initialize the global history register
    this->global_history = 0;

    // Check what type of branch predictor is being used and initialize the tables accordingly
    if(strcmp(bp_param.bp_name, "bimodal") == 0){
        // Initialize the bimodal table with the number of indexes = 2^M2
        this->bimodal_index_size = (int)std::pow(2, bp_param.M2);
        bimodal_table.resize(this->bimodal_index_size);

        // Initalize all branch history counters to 2 (weakly taken)
        for(int i = 0; i < bimodal_table.size(); i++){
            bimodal_table[i] = 2;
        }
    } else if(strcmp(bp_param.bp_name, "gshare") == 0){
        // Initialize the gshare table
        this->gshare_index_size = (int)std::pow(2, bp_param.M1);
        gshare_table.resize(this->gshare_index_size);

        // Initalize all branch history counters to 2 (weakly taken)
        for(int i = 0; i < gshare_table.size(); i++){
            gshare_table[i] = 2;
        }
    } else if(strcmp(bp_param.bp_name, "hybrid") == 0){
        // Initialize the hybrid table by creating a bimodal predictor and a gshare predictor
        
        // Initialize the bimodal predictor
        this->bimodal_index_size = (int)std::pow(2, bp_param.M2);
        bimodal_table.resize(this->bimodal_index_size);

        // Initalize all branch history counters to 2 (weakly taken)
        for(int i = 0; i < bimodal_table.size(); i++){
            bimodal_table[i] = 2;
        }

        // Initialize the gshare predictor
        this->gshare_index_size = (int)std::pow(2, bp_param.M1);
        gshare_table.resize(this->gshare_index_size);

        // Initalize all branch history counters to 2 (weakly taken)
        for(int i = 0; i < gshare_table.size(); i++){
            gshare_table[i] = 2;
        }

        // Initialize the hybrid chooser table of size 2^K 2 bit counters
        this->hybrid_index_size = (int)std::pow(2, bp_param.K); 
        hybrid_table.resize(this->hybrid_index_size);
        for(int i = 0; i < hybrid_table.size(); i++){
            hybrid_table[i] = 1;    // all counters in chooser table are initialized to 1 (weakly not taken)
        }
    }
}

void BranchHistoryTable::predict_bimodal_branch(int addr, char outcome){
    bool prediction;    // True if taken, False if not-taken

    // Update measurement counters
    this->number_of_predictions++;

    // Step 1: Determine the branch's index into the prediction table.
    // discard the lowest two bits of the PC, since these are always zero, i.e., use bits m+1 through 2 of the PC.
    int index = addr >> 2;

    // Mask bits m+1 through 2 of the PC
    index = index & ((1 << bp_param.M2) - 1);

    // Step 2: Make a prediction. Use index to get the branch’s counter from the prediction table. If the
    // counter value is greater than or equal to 2, then the branch is predicted taken, else it is predicted
    // not-taken.
    if(bimodal_table[index] >= 2){
        prediction = true;
    } else {
        prediction = false;
    }

    // Step 3: Update the branch predictor based on the branch’s actual outcome. The branch’s counter in
    // the prediction table is incremented if the branch was taken, decremented if the branch was not-
    // taken. The counter saturates at the extremes (0 and 3), however.
    if(outcome == 't'){
        if(!prediction){    // If the prediction is not taken, but the outcome is taken, then it is a misprediction
            // Update measurement counters
            this->number_of_mispredictions++;
        } 
        bimodal_table[index] = std::min(bimodal_table[index] + 1, 3);
    } else if(outcome == 'n'){
        if(prediction){    // If the prediction is taken, but the outcome is not-taken, then it is a misprediction
            // Update measurement counters
            this->number_of_mispredictions++;
        } 
        bimodal_table[index] = std::max(bimodal_table[index] - 1, 0);
    } 
}

void BranchHistoryTable::predict_gshare_branch(int addr, char outcome){
    // Local variables
    bool prediction = false;    // True if taken, False if not-taken
    int current_index = 0;
    int upper_n_bits = 0;
    int m_minus_n_bits = 0;
    int xor_result = 0;
    int concatenated_index = 0;
    bool N_is_zero = false;

    // Check if N is zero
    if(this->bp_param.N == 0){
        N_is_zero = true;
    }

    // Update measurement counters
    this->number_of_predictions++;

    // Step 1: Determine the branch’s index into the prediction table. Fig. 2 shows how to generate the
    // index: the current n-bit global branch history register is XORed with the uppermost n bits of the
    // m PC bits.

    // calculate m - n
    int M1 = this->bp_param.M1;
    int N = this->bp_param.N;
    m_minus_n_bits = M1 - N;

    // Discard the lowest two bits of the PC, since these are always zero, i.e., use bits m+1 through 2 of the PC.
    current_index = (addr >> 2);

    // Mask M1 bits from the PC
    current_index = current_index & ((1 << M1) - 1);

    if(!N_is_zero){
        // Mask the upper n bits of the PC (shift off m-n bits)
        upper_n_bits = current_index >> m_minus_n_bits;

        // XOR the global history register with the upper n bits of the PC
        xor_result = this->global_history ^ upper_n_bits;

        // Mask out the upper n bits from current_index
        current_index = current_index & ((1 << m_minus_n_bits) - 1);

        // concatenate the result with the lower m-n bits of the PC
        concatenated_index = xor_result << m_minus_n_bits;
    }
    concatenated_index = concatenated_index | current_index;

    // Step 2: Make a prediction. Use index to get the branch’s counter from the prediction table. If the
    // counter value is greater than or equal to 2, then the branch is predicted taken, else it is predicted
    // not-taken.
    if(gshare_table[concatenated_index] >= 2){
        prediction = true;
    } else {
        prediction = false;
    }

    // Step 3: Update the branch predictor based on the branch’s actual outcome. The branch’s counter in
    // the prediction table is incremented if the branch was taken, decremented if the branch was not-
    // taken. The counter saturates at the extremes (0 and 3), however.
    if(outcome == 't'){
        if(!prediction){    // If the prediction is not taken, but the outcome is taken, then it is a misprediction
            // Update measurement counters
            this->number_of_mispredictions++;
        } 
        gshare_table[concatenated_index] = std::min(gshare_table[concatenated_index] + 1, 3);
    } else if(outcome == 'n'){
        if(prediction){    // If the prediction is taken, but the outcome is not-taken, then it is a misprediction
            // Update measurement counters
            this->number_of_mispredictions++;
        } 
        gshare_table[concatenated_index] = std::max(gshare_table[concatenated_index] - 1, 0);
    }

    // Step 4: Update the global branch history register. Shift the register right by 1 bit position, and place
    // the branch’s actual outcome into the most-significant bit position of the register.
    int outcome_bit = 0;
    if(outcome == 't'){
        outcome_bit = 1;
    } else if(outcome == 'n'){
        outcome_bit = 0;
    } 

    this->global_history = (this->global_history >> 1) | (outcome_bit << (this->bp_param.N - 1)); 

}

void BranchHistoryTable::predict_hybrid_branch(int addr, char outcome){
    // Local variables
    char hybrid_prediction = '1';    // '1' if gshare is selected, '0' if bimodal is selected

    // Update measurement counters
    this->number_of_predictions++;

    // Step 1: Obtain two predictions, one from the bimodal predictor and one from the gshare predictor
    // gshare prediction
    bool gshare_prediction = false;    // True if taken, False if not-taken
    int M_index = 0;
    int upper_n_bits = 0;
    int m_minus_n_bits = 0;
    int xor_result = 0;
    int concatenated_index = 0;
    bool N_is_zero = false;

    // Check if N is zero
    if(this->bp_param.N == 0){
        N_is_zero = true;
    }

    // Step 1: Determine the branch’s index into the prediction table. 
    // calculate m - n
    int M1 = this->bp_param.M1;
    int N = this->bp_param.N;
    m_minus_n_bits = M1 - N;

    // Discard the lowest two bits of the PC, since these are always zero, i.e., use bits m+1 through 2 of the PC.
    M_index = (addr >> 2);

    // Mask M1 bits from the PC
    M_index = M_index & ((1 << M1) - 1);

    if(!N_is_zero){
        // Mask the upper n bits of the PC (shift off m-n bits)
        upper_n_bits = M_index >> m_minus_n_bits;

        // XOR the global history register with the upper n bits of the PC
        xor_result = this->global_history ^ upper_n_bits;

        // Mask out the upper n bits from M_index
        M_index = M_index & ((1 << m_minus_n_bits) - 1);

        // concatenate the result with the lower m-n bits of the PC
        concatenated_index = xor_result << m_minus_n_bits;
    }
    concatenated_index = concatenated_index | M_index;

    // Step 2: Make a prediction. 
    if(gshare_table[concatenated_index] >= 2){
        gshare_prediction = true;
    } else {
        gshare_prediction = false;
    }

    // Bimodal prediction
    bool bimodal_prediction = false;    // True if taken, False if not-taken

    // Step 1: Determine the branch's index into the prediction table.
    int bimodal_index = addr >> 2;

    // Mask bits m+1 through 2 of the PC
    bimodal_index = bimodal_index & ((1 << bp_param.M2) - 1);

    // Step 2: Make a prediction.
    if(bimodal_table[bimodal_index] >= 2){
        bimodal_prediction = true;
    } else {
        bimodal_prediction = false;
    }


    // Step 2: Determine the branch's index into the chooser table. The index for the chooser table is 
    // bit k+1 to bit 2 of the PC
    int chooser_index = addr >> 2;
    chooser_index = chooser_index & ((1 << bp_param.K) - 1);
    
    // Step 3: Make an overall prediction. Use the index to get the branch's counter from the chooser table. 
    // If the chooser counter value is greater than or equal to 2, then use the prediction that was obtained 
    // from the gshare predictor, otherwise use the prediction that was obtained from the bimodal predictor.

    if(hybrid_table[chooser_index] >= 2){
        hybrid_prediction = '1';    // gshare is selected
    } else {
        hybrid_prediction = '0';    // bimodal is selected
    }

    // Step 4: Update the selected branch predictor based on the branch's actual outcome. Only the branch 
    // predictor that was selected in step 3, above, is updated.

    if(hybrid_prediction == '1'){
        // Update the gshare predictor
        if(outcome == 't'){
            if(!gshare_prediction){    // If the prediction is not taken, but the outcome is taken, then it is a misprediction
                // Update measurement counters
                this->number_of_mispredictions++;
            } 
            gshare_table[concatenated_index] = std::min(gshare_table[concatenated_index] + 1, 3);
        } else if(outcome == 'n'){
            if(gshare_prediction){    // If the prediction is taken, but the outcome is not-taken, then it is a misprediction
                // Update measurement counters
                this->number_of_mispredictions++;
            } 
            gshare_table[concatenated_index] = std::max(gshare_table[concatenated_index] - 1, 0);
        }
    } else if(hybrid_prediction == '0'){
        // Update the bimodal predictor
        if(outcome == 't'){
            if(!bimodal_prediction){    // If the prediction is not taken, but the outcome is taken, then it is a misprediction
                // Update measurement counters
                this->number_of_mispredictions++;

            } 
            bimodal_table[bimodal_index] = std::min(bimodal_table[bimodal_index] + 1, 3);
        } else if(outcome == 'n'){
            if(bimodal_prediction){    // If the prediction is taken, but the outcome is not-taken, then it is a misprediction
                // Update measurement counters
                this->number_of_mispredictions++;
            } 
            bimodal_table[bimodal_index] = std::max(bimodal_table[bimodal_index] - 1, 0);
        } 
    }

    // Step 5: Note that the gshare global branch history register must always be updated, even if bimodal
    // was selected.
    int outcome_bit = 0;
    if(outcome == 't'){
        outcome_bit = 1;
    } else if(outcome == 'n'){
        outcome_bit = 0;
    } 

    this->global_history = (this->global_history >> 1) | (outcome_bit << (this->bp_param.N - 1)); 

    
    // Step 6: Update the branch's chooser counter. 
    // If both are incorrect, or both are correct, then no change is made to the chooser counter.

    // If gshare is correct and bimodal is incorrect, then the chooser counter is incremented but saturates at 3.
    if(((gshare_prediction == true && outcome == 't') || (gshare_prediction == false && outcome == 'n')) && 
    ((bimodal_prediction == false && outcome == 't') || (bimodal_prediction == true && outcome == 'n'))){
        hybrid_table[chooser_index] = std::min(hybrid_table[chooser_index] + 1, 3);
    }

    // If bimodal is correct and gshare is incorrect, then the chooser counter is decremented but saturates at 0.
    if(((gshare_prediction == false && outcome == 't') || (gshare_prediction == true && outcome == 'n')) && 
    ((bimodal_prediction == true && outcome == 't') || (bimodal_prediction == false && outcome == 'n'))){
        hybrid_table[chooser_index] = std::max(hybrid_table[chooser_index] - 1, 0);
    }
}

void BranchHistoryTable::print_contents(){
    std::cout << "OUTPUT" << std::endl;

    float misprediction_rate = 0;

    // Calculate the misprediction rate
    misprediction_rate = (static_cast<float>(this->number_of_mispredictions) / static_cast<float>(this->number_of_predictions)) * 100;

    // Print the measurement counters
    std::cout << "number of predictions: " << number_of_predictions << std::endl;
    std::cout << "number of mispredictions: " << number_of_mispredictions << std::endl;
    // Print the misprediction rate as a percentage with two decimal places
    std::cout << std::fixed << std::setprecision(2) << "misprediction rate: " << misprediction_rate << "%" << std::endl; 

    if(strcmp(this->bp_param.bp_name, "bimodal") == 0){
        std::cout << "FINAL BIMODAL CONTENTS" << std::endl;
        for(int i = 0; i < bimodal_table.size(); i++){
            std::cout << " " << i << "	" << bimodal_table[i] << std::endl;
        }
    } else if(strcmp(this->bp_param.bp_name, "gshare") == 0){
        std::cout << "FINAL GSHARE CONTENTS" << std::endl;
        for(int i = 0; i < gshare_table.size(); i++){
            std::cout << " " << i << "	" << gshare_table[i] << std::endl;
        }
    } else if(strcmp(this->bp_param.bp_name, "hybrid") == 0){
        std::cout << "FINAL CHOOSER CONTENTS" << std::endl;
        for(int i = 0; i < hybrid_table.size(); i++){
            std::cout << " " << i << "	" << hybrid_table[i] << std::endl;
        }

        std::cout << "FINAL GSHARE CONTENTS" << std::endl;
        for(int i = 0; i < gshare_table.size(); i++){
            std::cout << " " << i << "	" << gshare_table[i] << std::endl;
        }

        std::cout << "FINAL BIMODAL CONTENTS" << std::endl;
        for(int i = 0; i < bimodal_table.size(); i++){
            std::cout << " " << i << "	" << bimodal_table[i] << std::endl;
        }
    }
}


#endif
