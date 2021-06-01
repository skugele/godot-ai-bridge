extends Node2D

# a list of actions pending execution
onready var pending_actions = []

func _ready():
	pass
	
func _process(delta):
	var action = pending_actions.pop_front()
	if action:
		execute(action)

func add_action(action):
	pending_actions.push_back(action)
		
func execute(action):
	print('executing action: ', action)
	match action:
		'up' : 
			print('moving up!')
			global_position.y -= 10
		'down' :
			print('moving down!')
			global_position.y += 10
		'left':
			print('moving left!')
			global_position.x -= 10
		'right':
			print('moving right!')
			global_position.x += 10
		'rotate_clockwise':
			print('rotating clockwise!')
			rotation_degrees += 5.0
		'rotate_counterclockwise':
			print('rotating counterclockwise!')
			rotation_degrees -= 5.0
		
		# default case: unrecognized actions
		_:
			print('unrecogized action: ', action)
