{% if flag?(:gc_none) %}
  require "../src/immix"
{% end %}
require "http/server"

i = 0

server = HTTP::Server.new do |ctx|
  ctx.response.headers["content-type"] = "text/plain"
  ctx.response << "Hello World #{i += 1}\n"
end
server.bind_tcp(9292)

#spawn do
  puts "listening on :9292"
  server.listen
#end

#spawn do
#  client = HTTP::Client.new("localhost", 9292)
#  response = nil
#
#  loop do
#    response = client.get("/")
#  end
#
#  p response
#end
#
#Scheduler.reschedule
