extends Node2D

# a list of actions pending execution
onready var pending_actions = []

func _process(delta):
	var action = pending_actions.pop_front()
	if action:
		execute(action)

func add_action(action):
	pending_actions.push_back(action)
		
func execute(action):
	match action:
		'up': global_position.y -= 10
		'down': global_position.y += 10
		'left': global_position.x -= 10
		'right': global_position.x += 10
		'rotate_clockwise': rotation_degrees += 5.0
		'rotate_counterclockwise': rotation_degrees -= 5.0
		
		# default case: unrecognized actions
		_: print('unrecogized action: ', action) 
