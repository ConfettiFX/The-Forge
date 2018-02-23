import jenkins
import sys
	
server = jenkins.Jenkins('http://98.187.254.136:9000', username='forge_builder', password='skydome01')

server.build_job('The Forge', {'param1': 'BUILD'})
last_build_number = server.get_job_info('The Forge')['lastCompletedBuild']['number']
build_info = server.get_build_info('The Forge', last_build_number)

#sys.stderr.write(r"Please look at following job numbers on Jenkins:")
#sys.stderr.write(r"http://98.187.254.136:9000/job/The%20Forge/<build number here>")
#toPrint = "" + str(last_build_number +1) +", " + str(last_build_number + 2)
#sys.stderr.write(toPrint)

sys.exit(0)