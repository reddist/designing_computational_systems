#ifndef INC_KEYBOARD_H_
#define INC_KEYBOARD_H_

uint8_t Check_Row(uint8_t Nrow);
HAL_StatusTypeDef Set_Keyboard(void);
HAL_StatusTypeDef ks_continue(void);

extern int ks_state;
extern uint8_t ks_result, ks_current_row;

#endif /* INC_KEYBOARD_H_ */
