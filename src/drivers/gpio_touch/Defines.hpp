#pragma once
#include <string>                                  
#include <termios.h>

namespace stark_power_manager
{
	struct TouchOption                                 
	{                                                  
		std::string gpioChipPath{ "/dev/gpiochip2" };  
		int headLine{ 26 }; //头部触摸                           
		int chinLine{ 27 }; //下巴触摸                           
		int LeftEarLine{ 28 }; //左耳朵                           
		int RightEarLine{ 29 }; //右耳朵                           
	};
}// namespace stark_power_manager                                       
                                                 
