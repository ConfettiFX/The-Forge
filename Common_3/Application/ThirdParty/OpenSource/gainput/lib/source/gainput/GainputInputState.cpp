
#include "../../include/gainput/gainput.h"


namespace gainput
{

InputState::InputState(Allocator& allocator, unsigned int buttonCount) :
		allocator_(allocator),
		buttonCount_(buttonCount)
{
	const size_t size = sizeof(Button) * buttonCount_;
	buttons_ = static_cast<Button*>(allocator_.Allocate(size));
	GAINPUT_ASSERT(buttons_);
	memset(buttons_, 0, size);
}

InputState::~InputState()
{
	allocator_.Deallocate(buttons_);
}

InputState&
InputState::operator=(const InputState& other)
{
	const size_t size = sizeof(Button) * buttonCount_;
	memcpy(buttons_, other.buttons_, size);
	return *this;
}

}

