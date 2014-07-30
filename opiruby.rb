////////////////////////////////////////////////////////////////////////////////////////////////
//
// opiruby.rb
// Copyright (c) 2006 by Xie Yun.
//
////////////////////////////////////////////////////////////////////////////////////////////////

require "OPI.so"
require "Win32API"

#~ OPInvoke constants
INVOKE_ONEWAY = 0x00000000
INVOKE_ASYNC = 0x00000001
INVOKE_SYNC = 0x00008000
INVOKE_ACK = 0x80000000

#~ OPIAction constants
ACTIVE_ACTION = 0x00544341
PASSIVE_ACTION = 0x00565350
ACTIVATE_MANUAL = 0
ACTIVATE_IMMEDIATE = 1
ACTIVATE_LAZY = 2
ACTIVATE_ONREQUEST = 3
ACTSTATE_NONE = 0
ACTSTATE_WAIT = 1
ACTSTATE_RECEIVE = 2
ACTSTATE_RUN = 3
ACTSTATE_RETURN = 4
ACTSTATE_FINISH = 5

#~ Pentry constants
PETYPE_NONE = 0
PETYPE_THREAD = 0x00444854
PETYPE_WINDOW = 0x004E4955
PETYPE_TCP = 0x00504354
PETYPE_UDP = 0x00504455

#~ OPI constants
OPI_DEF_SIZE = 1024
OPI_DEF_INSTANCE_LIMIT = 1024
OPI_WAIT_TIMEOUT = -1

class OPIException < Exception
    attr_reader :what, :source

    def initialize( msg )
        if ( msg.empty? == true )
            @what = ""
            @source = ""
        else
            args = msg.split("\n")
            @what = args[0]
            @source = args[1]
        end
    end

    def set( msg )
        if ( msg.empty? == true )
            @what = ""
            @source = ""
        else
            args = msg.split("\n")
            @what = args[0]
            @source = args[1]
        end
        return self
    end

    def message
        "#{@what}\n#{@source}"
    end
    
    def to_s
        "#{@what}\n#{@source}"
    end
  end

module ModAction

    class OPIREQ
        attr_accessor :name, :except, :ios, :invoker, :signal
        
        def initialize( action_name,
            ios,
            invoker,
            signal,
            except
            )
            @name = action_name
            @except = except
            @ios = ios
            @invoker = invoker
            @signal = signal
        end

    end

    attr :pmain

    def initialize
        ObjectSpace.define_finalizer( self, proc{ freeAction( @impl ) } )
    end
    
    def activate( count = 1 )
        @impl.activate( count )
    end

    def bind( meth, acti = ACTIVATE_MANUAL )
        @impl.bind( meth, acti )
    end

    def delegate( to_pentity )
        @impl.delegate( to_pentity )
    end

    def delegateRelease
        @impl.delegateRelease
    end

    def activateMode
        @impl.activateMode
    end

    def except
        ex = OPIException.new( "" )
        @impl.except( ex )
    end

    def instanceCount
        @impl.instanceCount
    end

    def instanceLimit
        @impl.instanceLimit
    end

    def method
        @impl.method
    end

    def monitor
        @impl.monitor
    end
    
    def name
        @impl.name
    end

    def pentityLink
        @impl.pentityLink
    end

    def pentityMain
        @pmain
    end

    def request( req )
        return false if ( req == nil )
        return false if ( req.name != self.name )
        @m_thread = Thread.new( req ) { |t_req| handle( t_req ) }
        return true
    end

    def requestCount
        @impl.requestCount
    end

    def setActivateMode( acti = ACTIVATE_MANUAL )
        @impl.setActivateMode( acti )
    end

    def setInstanceLimit( limit = OPI_DEF_INSTANCE_LIMIT )
        @impl.setInstanceLimit( limit )
    end

    def setLinkPentity( name )
        @impl.setLinkPentity( name.to_s )
    end

    def setName( name )
        @impl.setName( name.to_s )
    end

    def setPentityMain( name )
        @pmain = name.to_s
    end

    def setStackSize( size = 0 )
        @impl.setStackSize( size )
    end

    def setType( type = ACTIVE_ACTION )
        @impl.setType( type )
    end

    def state( id = 0 )
        @impl.state( id )
    end

    def stop
        @impl.stop
    end

    def type
        @impl.type
    end

    def define( howto )
        @impl.define( howto )
    end
    
    def HOWTO
        howto = []
        @impl.HOWTO( howto )
    end
    
    def spec
        spec = []
        @impl.spec( spec )
    end
    
    def view( report )
        @impl.view( report )
    end
        
    def instance
        @impl.m_instance
    end

    def wrapFunc
        @impl.m_wrap_func
    end

    def handle( req )
        begin
            case self.method.class.to_s
                when 'Symbol'
                    send self.method, req.ios, req.invoker
                else 'Method'
                    self.method.call req.ios, req.invoker
            end
        rescue OPIException => ex
            @impl.setExcept( req, ex )
        rescue => ex
            t_ex = OPIException.new( ex.message )
            @impl.setExcept( req, t_ex )
        end
        @impl.finish  req.signal
    end

    private :handle

end

class OPIAction

    include ModAction

    def initialize( name,
        meth,
        type = ACTIVE_ACTION,
        acti = ACTIVATE_MANUAL,
        instance_limit = OPI_DEF_INSTANCE_LIMIT,
        stack_size = 0
        )
        @impl = OPIActionImpl.new( name.to_s,
                meth,
                type,
                acti,
                instance_limit,
                stack_size )
        ObjectSpace.define_finalizer( self, proc{ freeAction( @impl ) } )
    end

end

class OPIActionSingle

    include ModAction
    
    def initialize( name,
        meth,
        type = ACTIVE_ACTION,
        acti = ACTIVATE_MANUAL,
        stack_size = 0
        )
        @impl = OPIActionSingleImpl.new( name.to_s,
                meth,
                type,
                acti,
                stack_size )
        ObjectSpace.define_finalizer( self, proc{ freeAction( @impl ) } )
    end

end

class OPIOS

    def initialize( p_ios )
        @impl = OPIOSImpl.new( p_ios )
        ObjectSpace.define_finalizer( self, proc{ freeIOS( @impl ) } )
    end

    def availRead
        @impl.availRead
    end

    def availWrite
        @impl.availWrite
    end

    def ends
        @impl.ends
    end

    def flush
        @impl.flush
    end

    def isEnds
        @impl.isEnds
    end

    def setWaitTimeout( wait_ms )
        @impl.setWaitTimeout( wait_ms )
    end
    
    def read( size, wait_ms = OPI_WAIT_TIMEOUT )
        str = String.new
        if ( wait_ms == OPI_WAIT_TIMEOUT )
            @impl.read( str, size, wait_ms )
        else
            @impl.read( str, size )
        end
    end

    def readArray
        ary = []
        @impl.readArray( ary )
    end

    def readfOnce( write_func )
        @impl.readfOnce( write_func )
    end

    def readOnce( size )
        str = String.new
        @impl.readOnce( str, size )
    end

    def write( str, wait_ms = OPI_WAIT_TIMEOUT )
        if ( wait_ms == OPI_WAIT_TIMEOUT )
            @impl.write( str, str.length, wait_ms )
        else
            @impl.write( str, str.length )
        end
    end
    
    def writeArray( ary )
        @impl.writeArray( ary.clone )
    end

    def writefOnce( read_func )
        @impl.writefOnce( read_func )
    end

    def writeOnce( str )
        @impl.writeOnce( str, str.length )
    end

    def readObject
        res = String.new
        a = @impl.readObject(res)
        b = [ a ]
        c = b.pack("H*")
        d = Marshal.load( c )
    end
    
    def writeObject( obj )
        a = Marshal.dump( obj )
        b = a.unpack("H*")
        c = b.shift
        @impl.writeObject( c )
    end

end

class OPInvoke

    def initialize( obj_pentity,
        obj_action,
        invoker,
        in_size = OPI_DEF_SIZE,
        out_size = OPI_DEF_SIZE
        )
        @impl = OPInvokeImpl.new( obj_pentity,
                obj_action,
                invoker,
                in_size,
                out_size )
        @ios = OPIOS.new( @impl.IOS )
        ObjectSpace.define_finalizer( self, proc{ freeInvoke( @impl ) } )
    end

    def release
        @impl.release
    end
    
    def cancel
        @impl.cancel
    end

    def except
        ex = OPIException.new( "" )
        @impl.except( ex )
    end

    def execMode
        @impl.execMode
    end

    def execute( mode = INVOKE_SYNC + INVOKE_ACK, timeout = 0 )
        @impl.execute( mode, timeout )
    end

    def finished()
        @impl.finished
    end

    def invoker
        res = Pentry.new( "", 0, 0, 0 )
        @impl.invoker( res )
    end

    def IOS
        @ios
    end

    def isAccepted
        @impl.isAccepted
    end

    def objectAction
        str = String.new
        @impl.objectAction( str )
    end

    def objectPentity
        str = String.new
        @impl.objectPentity( str )
    end

    def returnValue
        @impl.returnValue
    end

    def setBufferSize( in_size, out_size )
        @impl.setBufferSize( in_size, out_size )
    end

    def setInvoker( invoker )
        @impl.setInvoker( invoker )
    end

    def setObjectAction( name )
        @impl.setObjectAction( name )
    end

    def setObjectPentity( name )
        @impl.setObjectPentity( name )
    end

end

class OPIPentity

    def initialize( name ) 
        @impl = OPIPentityImpl.new( name )
        ObjectSpace.define_finalizer( self, proc{ freePentity(@impl) } )
    end

    def close
        @impl.close
    end

    def getPentry
        res = Pentry.new( "", 0, 0, 0 )
        @impl.getPentry( res )
    end

    def name
        @impl.name
    end

    def open
        @impl.open
    end

    def query( args = nil )
        res = []
        @impl.query( res, args )
    end

end

class OPIPentityMain

    attr :actions

    def initialize( name )
        @actions = {}
        @m_sched = nil
        @impl = OPIPentityMainImpl.new( name )
        ObjectSpace.define_finalizer( self, proc{ freePentityMain( @imp ) } )
    end

    def add( name,
        meth,
        type = ACTIVE_ACTION,
        acti = ACTIVATE_MANUAL,
        instance_limit= OPI_DEF_INSTANCE_LIMIT,
        stack_size = 0
        )
        act = OPIAction.new( name,
                meth,
                type,
                acti,
                instance_limit,
                stack_size )
        self.addAction( act )
    end

    def addAction( act )
        @impl.addAction( act )
        @actions[ act.name ] = act
    end

    def addSingle( name,
        meth,
        type = ACTIVE_ACTION,
        acti = ACTIVATE_MANUAL,
        stack_size = 0
        )
        act = OPIActionSingle.new( name,
                meth,
                type,
                acti,
                stack_size )
        self.addActionSingle( act )
    end

    def addActionSingle( act )
        @impl.addActionSingle( act )
        @actions[ act.name ] = act
    end

    def batchDelegate( to_pentity )
        @impl.batchDelegate( to_pentity )
    end

    def batchDelegateRelease( to_pentity )
        @impl.batchDelegateRelease( to_pentity )
    end

    def close
        @impl.close( true )
        return true if ( @m_sched == nil )
        return false if ( @m_sched.alive? == false )
        @m_startup = false
        @m_sched.exit
        @m_sched.join
        return true
    end

    def count
        @impl.count
    end

    def find( name )
        return nil if ( @actions.include?( name ) == false )
        @actions[ name ]
    end

    def name
        @impl.name
    end

    def open
        return false if ( @m_sched.alive? == true ) if ( @m_sched != nil )
        @m_startup = true
        @m_sched = Thread.new { handle }
        @impl.open
    end

    def query( args = nil )
        res = String.new
        @impl.query( res, args )
    end

    def remove( name )
        return false if ( @actions.include?( name ) == false )
        @impl.remove( name )
        @actions.delete( name )
        return true
    end

    def view( report )
        @impl.view( report )
    end
    
    def handle
        while ( @m_startup == true )
            req = @impl.getRequest
            if ( req != nil )
                @actions[ req.name ].request( req ) if ( @actions.include?( req.name ) == true )
            else
              sleep 0.002
            end
        end
    end

    private :handle
end

class Pentry

    attr_accessor :name, :hostId, :entryType, :entry

    def initialize( name, hid = 0, etype = 0, entry = 0 )
        @name = name
        @hostId = hid
        @entryType = etype
        @entry = entry
    end

end

class MDCHead

    attr_accessor :type, :size, :count

    def initialize( type, size, count)
        @type = type
        @size = size
        @count = count
    end

end

def easyInvoc ( which_pentity,
    which_action,
    who,
    *args )
    
    params = ""
    if ( args.size == 1 )
        arg = args[0]
        if ( arg.kind_of?( Array ) )
            params = mdAssignRbArray( arg )
        else
            params = arg.to_s
        end
    elsif ( args.size > 1 )
        params = mdAssignRbArray( args )
    end
    param_len = params.length

    invoker = Pentry.new( who )
    invoc = OPInvoke.new( which_pentity, which_action, invoker )

    t_ios = invoc.IOS
    if ( param_len > 0 )
        w_len = t_ios.writeOnce( params )
    else
        w_len = 0
    end

    invoc.execute( INVOKE_ASYNC | INVOKE_ACK )
    res = ""
    while ( ! invoc.finished )
        w_len += t_ios.writeOnce( params[ w_len...param_len ] ) if ( t_ios.availWrite > 0 ) if ( w_len < param_len )
        r_len = t_ios.availRead
        res << t_ios.readOnce( r_len ) if ( r_len > 0 )
        sleep 0.002
    end
    r_len = t_ios.availRead
    res << t_ios.readOnce( r_len ) if ( r_len > 0 )
    ret_val = invoc.returnValue

    invoc.release
    return ret_val if ( res.size == 0 )
    mdCopyRbArray( res )
end
