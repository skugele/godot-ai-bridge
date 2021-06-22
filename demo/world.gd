# GDScript: world.gd

extends Node2D

# Godot-AI-Bridge (GAB) Variables
onready var gab = $GabLib  # library reference
onready var gab_options = {
	'publisher_port':10003, # specifies alternate port - default port is 10001
	'listener_port':10004, # specifies alternate port - default port is 10002
	
	# supported socket options (for advanced users - see ZeroMQ documentation for details)
	'socket_options': {
		'ZMQ_RCVHWM': 10,  # highwater mark for queued messages on receive
		'ZMQ_RCVTIMEO': 50, # timeout on I/O blocking for receive
		'ZMQ_SNDHWM': 10, 
		'ZMQ_SNDTIMEO': 50,
		'ZMQ_CONFLATE':1 
	},
	
	'verbosity': 2
}

func _ready():
	
	# initialize Godot-AI-Bridge
	gab.connect(gab_options)

	# initialize the publisher timer (send state frequency)	
	var publish_timer = Timer.new()
	
	publish_timer.wait_time = 0.5
	publish_timer.connect("timeout", self, "_on_publish_state")
	add_child(publish_timer)
	publish_timer.start()	
	
#######################
### SIGNAL HANDLERS ###
#######################

# signal handler for publish_timer's "timeout" signal
func _on_publish_state():
	for agent in $Agents.get_children():
		gab.send('/demo/agent/%s' % agent.id, agent.get_state())
	
# signal handler for Godot-AI-Bridge's "event_requested" signal
func _on_event_requested(event_details):
	print('Godot: event requested -> "%s"' % event_details)

	for agent in $Agents.get_children():
		if event_details['agent_id'] == agent.id:
			agent.add_action(event_details['action'])
