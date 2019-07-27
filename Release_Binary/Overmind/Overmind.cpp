// Overmind.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <ctime>
#include "AIStructs.h"

const float LR = 0.001;

int main(int argc, char* argv[])
{
	srand((unsigned int)time(0));
	if (argc < 3) {
		std::cout << "Overmind\n"; 
		std::cout << "-create name\n"; 
		exit(0);
	}
	Model m = {};
	if (std::string(argv[1]) == "-create") {
		saveModel(m, argv[2]);
	}
	else if (std::string(argv[1]) == "-show") {
		std::cout << "Showing " << argv[2] << std::endl;
		loadModel(m, argv[2]);
		std::cout << m << std::endl;
	}
	else if (std::string(argv[1]) == "-update") {
		if (argc < 5) {
			std::cout << "-update model result_list_file model_out" << std::endl;
			exit(0);
		}
		Model m = Model();
		loadModel(m, argv[2]);
		std::ifstream infile(argv[3]);
		std::string line;
		while (std::getline(infile, line)) {
			std::cout << "Updating from " << line << std::endl;
			Model c = Model();
			loadModel(c, line);
			for (int frame = 0; frame < c.get_frames(); ++frame) {
				auto grads = c.saved_grads(frame);
				m.descent(grads, c.winner ? LR : (-LR));
				std::cout << "Mean grads * lr: ";
				for (auto& grad : grads) {
					 std::cout << LR*grad.mean() << " : ";
				}
				std::cout << std::endl;
			}
		}
		saveModel(m, std::string(argv[4]) + "_updated");
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
