# GDScript: world.gd

extends Node2D

###################################
# Godot-AI-Bridge (GAB) Variables #
###################################
onready var gab = $GabLib  # library reference
onready var gab_options = {
	'publisher_port': 10003, # specifies alternate port - default port is 10001
	'listener_port': 10004, # specifies alternate port - default port is 10002
	
	# supported socket options (for advanced users - see ZeroMQ documentation for details)
	'socket_options': {
		'ZMQ_RCVHWM': 10,  # receive highwater mark
		'ZMQ_RCVTIMEO': 50,  # timeout on receive I/O blocking
		'ZMQ_SNDHWM': 10,  # send highwater mark
		'ZMQ_SNDTIMEO': 50,  # timeout on send I/O blocking
		'ZMQ_CONFLATE': 0  # only keep last message in send/receive queues (others are dropped)
	},
	
	# controls Godot-AI-Bridge's console verbosity level (larger numbers -> greater verbosity)
	'verbosity': 1   # supported values (-1=FATAL; 0=ERROR; 1=INFO; 2=WARNING; 3=DEBUG; 4=TRACE)
}


func _ready():
	
	# initialize Godot-AI-Bridge
	gab.connect(gab_options)

	# initializes a timer that controls the frequency of environment state broadcasts
	_create_publish_timer(0.1)


func _create_publish_timer(wait_time):
	var publish_timer = Timer.new()

	publish_timer.wait_time = wait_time  
	publish_timer.connect("timeout", self, "_on_publish_state")
	add_child(publish_timer)
	publish_timer.start()


#######################
### SIGNAL HANDLERS ###
#######################

# signal handler for publish_timer's "timeout" signal
func _on_publish_state():
	for agent in $Agents.get_children():
		
		# topics characterize message content. recipients can use topics to filter messages (e.g., by agent id)
		var topic = '/demo/agent/%s' % agent.id
		
		# Godot-AI-Bridge wraps this state into the "data" element of a JSON-encoded message. messages are also
		# given a "header" element containing a unique sequence numbers (seqno) and timestamp in milliseconds
		var msg = agent.get_state()
		
		# broadcasts the message to all clients
		gab.send(topic, msg)
	
# signal handler for Godot-AI-Bridge's "event_requested" signal
func _on_event_requested(event_details):
	print('Godot Environment: event request received -> "%s"' % event_details)

	var event = event_details['data']['event']	
	match event['type']:
		'action':
			# apply action to all agents with matching id
			for agent in $Agents.get_children():
				# adds action to an agent's pending action queue
				if event['agent'] == agent.id:				
					agent.add_action(event['value'])
					
		# default case: unrecognized actions
		_: print('unrecogized event type: ', event['type']) 
