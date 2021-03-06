
module CompTree
  #
  # minimal version of standard Queue
  #
  class Queue
    def initialize
      @queue = []
      @waiting = []
      @mutex = Mutex.new
    end
    
    def push(object)
      @mutex.synchronize {
        @queue.push object
        if thread = @waiting.shift
          thread.wakeup
        end
      }
    end

    def pop
      @mutex.synchronize {
        while true
          if @queue.empty?
            @waiting.push Thread.current
            @mutex.sleep
          else
            return @queue.shift
          end
        end
      }
    end
  end
end
