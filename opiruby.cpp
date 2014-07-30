////////////////////////////////////////////////////////////////////////////////////////////////
//
// opiruby.cpp
// Copyright (c) 2006 by Xie Yun. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4251)
#pragma warning(disable:4275)

#include "opiruby.h"
#include <stack>
#include <iostream>

// common functions

template< typename T >
inline T *
instance( VALUE obj )
{
	VALUE id = rb_iv_get( obj, "@m_instance" );
	if ( TYPE( id ) == T_NIL )
		throw OPIException( "invalidParameter" );

	return reinterpret_cast<T *>(NUM2UINT( id ));
}

// ActRbInvoc
////////////////////////////////////////////////////////////////////////////////////////////////

void
ActRbInvoc::handle( ActInstance & t_ins,
	const MDCHead & req,
	void * p )
{
	WrapInvoke & w_invoc = reinterpret_cast<WrapInvoke &>(const_cast<MDCHead &>(req));
	try
	{
		System::MsgQueue mq = t_ins.m_msg_queue;
		w_invoc.m_mq = mq;
		w_invoc.m_p_invoke = new (OPI::withrow) Invoke( w_invoc.m_args.obj_pentity,
				w_invoc.m_args.obj_action,
				Pentry( w_invoc.m_args.invoker ),
				w_invoc.m_args.in_size,
				w_invoc.m_args.out_size );
		w_invoc.m_p_wios = new WrapIOS( w_invoc.m_p_invoke->IOS(), w_invoc.m_mq );

		Int64_t l = { 0L, 0L };
		System::Message msg( 0, &l, sizeof(l) );
		System::MessageList msg_buf( 0 );
		UInt32 op = OP_NONE;
		System::WaitMode w_mode( OPI::def_wait_1 );
		bool b_fin = false;
		while ( ! b_fin )
		{
			if ( System::waitMessage( mq, NULL, &w_mode ) == OPI::WAIT_TIME_OUT )
			{
				b_fin = ( w_invoc.m_p_invoke == NULL
						|| w_invoc.m_p_wios == NULL );
				continue;
			}
			while ( System::recvMessage( mq, msg ) )
			{
				l.v.int_64 = *static_cast<Int64 *>(msg.m_data.p);
				switch ( msg.m_type )
				{
				case OPIMSG_ACTION:
					op = l.v.int_32[0];
					if ( op > OP_LAST )
					{
						msg_buf.push_back( msg );
						break;
					}
					System::sendMessageList( mq, msg_buf );
					if ( op < OP_IOS_FIRST )
					{
						w_invoc.opHandler( op,
								reinterpret_cast<void *>(l.v.int_32[1]) );
					}
					else
					{
						w_invoc.m_p_wios->opHandler( op,
								reinterpret_cast<void *>(l.v.int_32[1]) );
					}
					break;

				case OPIMSG_STOP:
					b_fin = true;
					break;

				default:
					msg_buf.push_back( msg );
					break;
				}
			}
		}
	}
	catch ( OPIException & ex )
	{
		this->setExcept( ex );
	}
	catch ( ... )
	{
		this->setExcept( OPIException( "unknownException", "ActRbInvoc::handle" ) );
	}
	OPI::freePtr( w_invoc.m_p_invoke );
	OPI::freePtr( w_invoc.m_p_wios );
}

WrapInvoke *
ActRbInvoc::invoke( const string & obj_pentity,
	const string & obj_action,
	const Pentry & invoker,
	UInt32 in_size,
	UInt32 out_size )
{
	WrapInvoke * p_invoc = NULL;
	try
	{
		p_invoc = new WrapInvoke( obj_pentity,
				obj_action,
				invoker,
				in_size,
				out_size);

		this->request( *reinterpret_cast<MDCHead *>(p_invoc) );
		while ( p_invoc->m_p_invoke == NULL
				|| p_invoc->m_p_wios == NULL )
		{
			rb_thread_sleep( OPI::def_wait/1000.0 );
			if ( ! this->except().empty() )
				throw this->except();
		}
	}
	catch ( OPIException & ex )
	{
		OPI::freePtr( p_invoc );
		ex.appendSource( "ActRbInvoc::invoke" );
		throw ex;
	}
	catch ( ... )
	{
		OPI::freePtr( p_invoc );
		throw OPIException( "unknownException", "ActRbInvoc::invoke" );
	}
	return p_invoc;
}

// WrapFunctor
////////////////////////////////////////////////////////////////////////////////////////////////

Int32
WrapFunctor::actFunc( BStream & t_ios,
	const Pentry & invoker )
{
	if ( this->m_pmrq == NULL )
		throw OPIException( "invalidPentityMain" );

	System::MsgQueue mq = System::openMsgQueue( System::getCurrentThreadId() );
	auto_ptr<WrapIOS> p_wios ( new WrapIOS( t_ios, mq ) ); 
	auto_ptr<OPIPMRQ::OPIREQ> p_req ( new OPIPMRQ::OPIREQ( this->m_action,
			p_wios.get(),
			&invoker,
			mq ) );

	this->m_pmrq->put( *p_req );
	Int64_t l = { 0L, 0L };
	System::Message msg( 0, &l, sizeof(l) );
	EnumOperation op = OP_NONE;
	bool b_fin = false;
	while ( ! b_fin )
	{
		System::waitMessage( mq, &msg );
		switch ( msg.m_type )
		{
		case OPIMSG_ACTION:
			l.v.int_64 = *static_cast<Int64 *>(msg.m_data.p);
			op = static_cast<EnumOperation>(l.v.int_32[0]);
			if ( op == OP_ACT_FINISH )
			{
				b_fin = true;
			}
			else
			if ( op >= OP_IOS_FIRST
				 || op <= OP_IOS_LAST )
			{
				p_wios->opHandler( op,
						reinterpret_cast<void *>(l.v.int_32[1]) );
			}
			break;
		
		case OPIMSG_STOP:
			b_fin = true;
			break;
		
		default:
			break;
		}
	}
	if ( ! p_req->m_except.empty() )
		throw p_req->m_except;

	return 0;
}

Int32
WrapFunctor::ioFunc( SBlock & s_b )
{
	VALUE ret = 0;
	try
	{
		VALUE r_str = rb_str_new( reinterpret_cast<char *>(s_b.p), s_b.size );
		ret = rb_funcall( this->m_method,
				rb_intern( "call" ),
				1,
				r_str );
	}
	OPI_CATCH( "WrapFunctor::ioFunc" );
	return ret;
};

// WrapInvoke
////////////////////////////////////////////////////////////////////////////////////////////////

void
WrapInvoke::opHandler( UInt32 op, void * p )
{
	if ( p == NULL )
		return;

	MDComposite & args = *static_cast<MDComposite *>(p);
	auto_ptr<Result> p_res ( new (OPI::withrow) Result( NULL, OPARG_SIZE ) );
	Int32 ret = 0;
	try
	{
		switch ( op )
		{
		case OP_INVOC_CANCEL:
			this->m_p_invoke->cancel();
			break;

		case OP_INVOC_EXEC:
			this->m_p_invoke->execute( *static_cast<UInt32 *>(mdeData( *args[0] )),
					*static_cast<UInt32 *>(mdeData( *args[1] )) );
			break;

		case OP_INVOC_FINISH:
			ret = this->m_p_invoke->finished();
			break;

		default:
			throw OPIException( "invalidOperation" );
			break;
		}
	}
	catch ( OPIException & ex )
	{
		p_res->setExcept( ex );
	}
	catch ( ... )
	{
		p_res->setExcept( OPIException( "unknownException" ) );
	}
	p_res->setReturnValue( ret );
	args.clear();
	args.assign( *p_res );
	this->m_sig.raise();
}

Int32
WrapInvoke::opRun( UInt32 op, void * p_args )
{
	if ( op == OP_NONE )
		throw OPIException( "invalidParameter" );

	if ( this->m_mq == 0 )
		return FAIL;

	Int64_t l = { 0L, 0L };
	l.v.int_32[0] = op;
	l.v.int_32[1] = reinterpret_cast<UInt32>(p_args);
	System::Message msg( OPIMSG_ACTION, &l, sizeof(l) );
	System::sendMessage( this->m_mq, msg );
	Int32 ret = this->m_sig.wait( OPI::WAIT_INFINITE );
	
	Result * p_res = reinterpret_cast<Result *>(p_args);
	if ( ! p_res->except().empty() )
		throw p_res->except();

	return p_res->returnValue();
}

void
WrapInvoke::release()
{
	if ( this->m_mq )
	{
		System::sendMessage( this->m_mq, System::Message( OPIMSG_STOP ) );
	}
}

// WrapIOS
////////////////////////////////////////////////////////////////////////////////////////////////

void
WrapIOS::opHandler( UInt32 op, void * p )
{
	if ( p == NULL )
		return;

	MDComposite & args = *static_cast<MDComposite *>(p);
	UInt32 arg_0 = 0;
	auto_ptr<Result> p_res ( new (OPI::withrow) Result( NULL, OPARG_SIZE ) );
	Int32 ret = 0;
	try
	{
		switch ( op )
		{
		case OP_IOS_ENDS:
			this->m_p_ios->ends();
			break;

		case OP_IOS_FLUSH:
			this->m_p_ios->flush();
			break;

		case OP_IOS_READ:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			this->m_p_ios->read( reinterpret_cast<void *>(arg_0),
					*static_cast<UInt32 *>(mdeData( *args[1] )),
					*static_cast<UInt32 *>(mdeData( *args[2] )) );
			break;

		case OP_IOS_READMDC:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			(*this->m_p_ios) >> *reinterpret_cast<MDComposite *>(arg_0);
			break;

		case OP_IOS_READONCE:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			ret = this->m_p_ios->readOnce( reinterpret_cast<void *>(arg_0),
					*static_cast<UInt32 *>(mdeData( *args[1] )) );
			break;

		case OP_IOS_READFONCE:
			break;

		case OP_IOS_WRITE:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			this->m_p_ios->write( reinterpret_cast<void *>(arg_0),
					*static_cast<UInt32 *>(mdeData( *args[1] )),
					*static_cast<UInt32 *>(mdeData( *args[2] )) );
			break;

		case OP_IOS_WRITEMDC:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			(*this->m_p_ios) << *reinterpret_cast<MDComposite *>(arg_0);
			break;

		case OP_IOS_WRITEONCE:
			arg_0 = *static_cast<UInt32 *>(mdeData( *args[0] ));
			ret = this->m_p_ios->writeOnce( reinterpret_cast<void *>(arg_0),
					*static_cast<UInt32 *>(mdeData( *args[1] )) );
			break;

		case OP_IOS_WRITEFONCE:
			break;

		default:
			throw OPIException( "invalidOperation" );
			break;
		}
	}
	catch ( OPIException & ex )
	{
		p_res->setExcept( ex );
	}
	catch ( ... )
	{
		p_res->setExcept( OPIException( "unknownException" ) );
	}
	p_res->setReturnValue( ret );
	args.clear();
	args.assign( *p_res );
	this->m_sig.raise();
}

Int32
WrapIOS::opRun( UInt32 op, void * p_args )
{
	if ( op == OP_NONE )
		throw OPIException( "invalidParameter" );

	if ( this->m_mq == 0 )
		return FAIL;

	Int64_t l = { 0L, 0L };
	l.v.int_32[0] = op;
	l.v.int_32[1] = reinterpret_cast<UInt32>(p_args);
	System::Message msg( OPIMSG_ACTION, &l, sizeof(l) );
	System::sendMessage( this->m_mq, msg );
	Int32 ret = 0;
	if ( op >= OP_IOS_FIRST
		 && op <= OP_IOS_LAST )
	{
		ret = this->m_sig.wait( this->m_p_ios->In().waitTimeout() );
	}
	else
	{
		ret = this->m_sig.wait( this->m_p_ios->Out().waitTimeout() );
	}
	if ( ret == OPI::WAIT_TIME_OUT )
		throw OPIException( "timeout" );

	Result * p_res = reinterpret_cast<Result *>(p_args);
	if ( ! p_res->except().empty() )
		throw p_res->except();

	return p_res->returnValue();
}

// OPI Meta-data (OMD)
////////////////////////////////////////////////////////////////////////////////////////////////

void
assignMDCH( MDCHead & dest, VALUE & src )
{
	dest.size = NUM2UINT( rb_funcall( src, rb_intern( "size" ), 0) );
	dest.type = static_cast<OPI::Enum::MDataType>(NUM2UINT( rb_funcall( src,
			rb_intern( "type" ),
			0 ) ));
	dest.count = NUM2UINT( rb_funcall( src,
			rb_intern( "count" ),
			0 ) );
}

void
assignMDEBig( MDElement & dest, VALUE src )
{
	UInt32 sz = NUM2UINT( rb_funcall( src, rb_intern( "size" ), 0 ) );
	auto_ptr<Int8> buf ( new char[sz + 1] );
	Int8 * p_buf = buf.get();
	::memset( p_buf, 0, sz + 1 );
	for( UInt32 i = 0 ;i < sz ;i++ )
	{
		p_buf[i] = 0;
		for( UInt8 j = 0, bit = 1 ;j < 8 ;j++, bit <<= 1 )
		{
			UInt8 plus = NUM2INT( rb_funcall( src,
					rb_intern( "[]" ),
					1,
					UINT2NUM( (i<<3) + j ) ) );
			if ( plus )
				p_buf[i] += bit;
		}
	}
	dest.assign( p_buf, sz );
}

void
assignMDE( MDElement & dest, VALUE src )
{
	try
	{
		bool b = false;
		Int32 i = 0L;
		double d = 0.0;
		switch ( TYPE( src ) )
		{
		case T_NIL:
			dest.assign( null_mde );
			break;

		case T_STRING:
			dest.assign( RSTRING(src)->ptr, RSTRING(src)->len );
			break;

		case T_TRUE:
			b = true ;
			dest.assign( &b, sizeof(b) );
			break;

		case T_FALSE:
			b = false ;
			dest.assign( &b, sizeof(b) );
			break;

		case T_FIXNUM:
			i = FIX2LONG( src );
			dest.assign( &i, sizeof(i) );
			break;

		case T_FLOAT:
			d = NUM2DBL( src );
			dest.assign( &d, sizeof(d) );
			break;

		case T_BIGNUM:
			assignMDEBig( dest, src );
			break;

		default:
			throw OPIException( "invalidParameter" );
			break;
		}
	}
	OPI_CATCH( "OPIRuby::assignMDE" );
}

void
assignMDC( MDComposite & dest, VALUE src )
{
	try
	{
		dest.clear();
		MDElement mde( OPI_DEF_SIZE_1 );
		if ( TYPE( src ) != T_ARRAY )
			throw OPIException( "invalidParameter(Array)" );

		while( true )
		{
			VALUE count = rb_funcall( src,
					rb_intern( "size" ),
					0 );

			if ( NUM2UINT( count ) == 0 )
				break;

			VALUE child = rb_ary_shift( src );
			if ( TYPE( child ) != T_ARRAY )
			{
				if ( TYPE( child ) == T_STRING )
				{
					if ( RSTRING(child)->len )
					{
						assignMDE( mde, child );
						dest.append( mde );
					}
					else
						dest.append( null_mde.block() );
				}
				else
				{
					assignMDE( mde, child );
					dest.append( mde );
				}
			}
			else
			{
				MDComposite mdc( NULL, OPI_DEF_SIZE_1 );
				assignMDC( mdc, child );
				dest.append( mdc );
			}
		}
	}
	OPI_CATCH( "OPIRuby::assignMDC" )
}

void
assignRbItem( VALUE & dest, const MDElement & src )
{
	try
	{
		string s;
		src.copy( s );
		dest = rb_str_new( s.c_str(), s.size() );
	}
	OPI_CATCH( "OPIRuby::assignRbItem" );
}

void
assignRbArray( VALUE & dest, const MDComposite & src )
{
	try
	{
		MDCHead * child = NULL;
		UInt32 I = src.count();
		for( UInt32 i = 0;i < I;++i )
		{
			child = src[i];
			if ( child == NULL )
				return;

			UInt32 count = child->count;
			switch( child->type )
			{
			case MDTYPE_ELEMENT:
				{
					string s( static_cast<char *>(mdeData( *child )), child->size );
					VALUE r_s = rb_str_new( s.c_str(), s.size() );
					RBGCREG( r_s );
					rb_ary_push( dest, r_s );
					RBGCUREG( r_s );
				}
				break;

			case MDTYPE_COMPOSITE:
				{
					MDComposite son( *child );
					VALUE r_ary = rb_ary_new();
					RBGCREG( r_ary );
					assignRbArray( r_ary, son );
					rb_ary_push( dest, r_ary );
					RBGCUREG( r_ary );
				}
				break;

			default:
				throw OPIException( "invalidParameter" );
				break;
			}
		}
	}
	OPI_CATCH( "OPIRuby::assignRbArray" );
}

void
assignRbPentry( VALUE dest, const Pentry & src )
{
	rb_iv_set( dest,
			"@name",
			rb_str_new2( src.name().c_str() ) );
	rb_iv_set( dest,
			"@entryType",
			UINT2NUM( src.entryType() ) );
	rb_iv_set( dest,
			"@entry",
			UINT2NUM( src.entry() ) );

	VALUE r_s;
	switch ( src.hostIdSize() )
	{
	case sizeof(UInt32):
		rb_iv_set( dest,
				"@hostId",
				UINT2NUM( src.hostId() ) );
		break;

	case sizeof(Int64_t):
		r_s = rb_str_new( reinterpret_cast<char *>(&src.hostId64()), sizeof(Int64_t) );
		rb_iv_set( dest,
				"@hostId",
				rb_funcall( r_s, rb_intern( "to_i" ), 0 ) );
		break;

	case sizeof(Int128_t):
		r_s = rb_str_new( reinterpret_cast<char *>(&src.hostId128()), sizeof(Int128_t) );
		rb_iv_set( dest,
				"@hostId",
				rb_funcall( r_s, rb_intern( "to_i" ), 0 ) );
		break;

	default:
		throw OPIException( "badFormat(Pentry)" );
		break;
	}
}

static VALUE
mdAssignRbArray( VALUE self, VALUE src )
{
	VALUE ret = Qnil;
	try
	{
		if ( TYPE( src ) != T_ARRAY )
			throw OPIException( "invalidParameter" );

		MDComposite mdc;
		assignMDC( mdc, src );
		char * p_ch = static_cast<char *>(mdc.blockPtr());
		ret = rb_str_new( static_cast<char *>(p_ch), mdBSize( p_ch ) );
	}
	OPI_CATCH( "mdAssignRbArray" );
	return ret;
}

static VALUE
mdCopyRbArray( VALUE self, VALUE src )
{
	VALUE ret = Qnil;
	try
	{
		void * p = RSTRING(src)->ptr;
		if ( !isMDType( p ) )
			return src;

		MDComposite mdc( *static_cast<MDCHead *>(p) );
		ret = rb_ary_new();
		assignRbArray( ret, mdc );
	}
	OPI_CATCH( "mdCopyRbArray" );
	return ret;
}

// OPI::PEntity::Action
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
actionNew( VALUE self,
	VALUE name,
	VALUE meth,
	VALUE type,
	VALUE acti,
	VALUE instance_limit,
	VALUE stack_size )
{
	try
	{
		if ( TYPE( self ) == T_NIL ||
			 TYPE( name ) != T_STRING ||
			 meth == Qnil ||
			 TYPE( type ) != T_FIXNUM ||
			 TYPE( acti ) != T_FIXNUM ||
			 TYPE( instance_limit ) != T_FIXNUM ||
			 TYPE( stack_size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		string nm ( STR2CSTR( name ) );
		WrapFunctor * p_wf = new WrapFunctor( meth );
		auto_ptr<ActionFunctor> p_af ( new ActionFunctor( *p_wf, &WrapFunctor::actFunc ) );
		Action * p_act = new (OPI::withrow) Action( nm,
				p_af.get(),
				static_cast<OPI::Enum::ActionType>(NUM2UINT( type )),
				static_cast<OPI::Enum::ActionActivate>(NUM2UINT( acti )),
				NUM2UINT( instance_limit ),
				NUM2UINT( stack_size ) );

		p_wf->setAction( nm );
		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_act) ) );
		rb_iv_set( self,
				"@m_wrap_func",
				UINT2NUM( reinterpret_cast<UInt32>(p_wf) ) );
	}
	OPI_CATCH( "OPIRuby::actionNew" );
	return self;
}

static VALUE
actionActivate( VALUE self, VALUE count )
{
	try
	{
		if ( TYPE( count ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->activate( NUM2UINT( count ) );
	}
	OPI_CATCH( "OPIRuby::actionActivate" );
	return Qnil;
}

static VALUE
actionBind( VALUE self,
	VALUE meth,
	VALUE acti )
{
	try
	{
		if ( meth == Qnil ||
			 TYPE( acti ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		auto_ptr<WrapFunctor> p_wf( new WrapFunctor( meth ) );
		ActionFunctor * p_af = new ActionFunctor( *p_wf, &WrapFunctor::actFunc );
		p_act->bind( *p_af, static_cast<OPI::Enum::ActionActivate>(NUM2UINT( acti )) );
	}
	OPI_CATCH( "OPIRuby::actionBind" );
	return Qnil;
}

static VALUE
actionDelegate( VALUE self, VALUE to_pentity )
{
	try
	{
		if ( to_pentity == Qnil || TYPE( to_pentity ) != T_STRING )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->delegate( string( STR2CSTR( to_pentity ) ) );
	}
	OPI_CATCH( "OPIRuby::actionDelegate" );
	return Qnil;
}

static VALUE
actionDelegateRelease( VALUE self )
{
	try
	{
		Action * p_act = instance<Action>( self );
		p_act->delegateRelease();
	}
	OPI_CATCH( "OPIRuby::actionDelegateRelease" );
	return Qnil;
}

static VALUE
actionActivateMode( VALUE self )
{
	VALUE ret = 0;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->activateMode() );
	}
	OPI_CATCH( "OPIRuby::actionActivateMode" );
	return ret;
}

static VALUE
actionExcept( VALUE self, VALUE r_ex )
{
	try
	{
		Action * p_act = instance<Action>( self );
		const OPIException & ex = p_act->except();
		rb_iv_set( r_ex,
				"@what",
				rb_str_new2( ex.what().c_str() ) );
		rb_iv_set( r_ex,
				"@source",
				rb_str_new2( ex.source().c_str() ) );
	}
	OPI_CATCH( "OPIRuby::actionExcept" );
	return r_ex;
}

static VALUE
actionFinish( VALUE self,
	VALUE r_sig )
{
	System::MsgQueue mq = static_cast<System::MsgQueue>(NUM2UINT( r_sig ));
	Int64_t l = { 0L, 0L };
	l.v.int_32[0] = OP_ACT_FINISH;
	System::Message msg( OPIMSG_ACTION, &l, sizeof(l) );
	System::sendMessage( mq, msg );
	return Qnil;
}

static VALUE
actionGetMethod( VALUE self )
{
	WrapFunctor * p_wf = NULL;
	try
	{
		Action * p_act = instance<Action>( self );
		ActionFunctor * p_f = p_act->functor();
		p_wf = static_cast<WrapFunctor *>(const_cast<void *>(p_f->objectPtr()));
	}
	OPI_CATCH( "OPIRuby::actionGetFunc" );
	if ( p_wf == NULL )
		return Qnil;

	return p_wf->getMethod();
}

static VALUE
actionInstanceCount( VALUE self )
{
	VALUE ret = 0;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->instanceCount() );
	}
	OPI_CATCH( "OPIRuby::actionInstanceCount" );
	return ret;
}

static VALUE
actionInstanceLimit( VALUE self )
{
	VALUE ret = 0;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->instanceLimit() ) ;
	}
	OPI_CATCH( "OPIRuby::actionInstanceLimit" );
	return ret;
}

static VALUE
actionMonitor( VALUE self )
{
	try
	{
		Action * p_act = instance<Action>( self );
		p_act->monitor();
	}
	OPI_CATCH( "OPIRuby::actionMonitor" );
	return Qnil;
}

static VALUE
actionName( VALUE self )
{
	VALUE ret = Qnil;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = rb_str_new2( p_act->name().c_str() );
	}
	OPI_CATCH( "OPIRuby::actionName" );
	return ret;
}

static VALUE
actionPentityLink( VALUE self )
{
	VALUE ret = Qnil;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = rb_str_new2( p_act->pentityLink().c_str() );
	}
	OPI_CATCH( "OPIRuby::actionPentityLink" );
	return ret;
}

static VALUE
actionRequestCount( VALUE self)
{
	VALUE ret = 0;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->requestCount() );
	}
	OPI_CATCH( "OPIRuby::actionRequestCount" );
	return ret;
}

static VALUE
actionSetActivateMode( VALUE self, VALUE acti )
{
	try
	{
		if ( TYPE( acti ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setActivateMode( static_cast<OPI::Enum::ActionActivate>(NUM2UINT( acti )) );
	}
	OPI_CATCH( "OPIRuby::actionSetActivateMode" );
	return Qnil;
}

static VALUE
actionSetExcept( VALUE self,
	VALUE req,
	VALUE r_ex )
{
	VALUE r_w = rb_iv_get( r_ex, "@what" );
	VALUE r_s = rb_iv_get( r_ex, "@source" );
	OPIException * p_ex = reinterpret_cast<OPIException *>(NUM2UINT( rb_iv_get( req, "@except" ) ));
	if ( p_ex != NULL )
	{
		*p_ex = OPIException( STR2CSTR( r_w ), STR2CSTR( r_s ) );
	}
	return Qnil;
}

static VALUE
actionSetInstanceLimit( VALUE self, VALUE limit )
{
	try
	{
		if ( TYPE( limit ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setInstanceLimit( NUM2UINT( limit ));
	}
	OPI_CATCH( "OPIRuby::actionSetInstanceLimit" );
	return Qnil;
}

static VALUE
actionSetPentityLink( VALUE self, VALUE name )
{
	try
	{
		if ( TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setPentityLink( string( STR2CSTR( name ) ) );
	}
	OPI_CATCH( "OPIRuby::actionSetPentityLink" );
	return Qnil;
}

static VALUE
actionSetName( VALUE self, VALUE name )
{
	try
	{
		if ( name == Qnil || TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setName( string( STR2CSTR( name )));
	}
	OPI_CATCH( "OPIRuby::actionSetName" );
	return Qnil;
}

static VALUE
actionSetStackSize( VALUE self, VALUE size )
{
	try
	{
		if ( TYPE( size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setStackSize( NUM2UINT( size ) );
	}
	OPI_CATCH( "OPIRuby::actionSetStackSize" );
	return Qnil;
}

static VALUE
actionSetType( VALUE self, VALUE type )
{
	try
	{
		if ( TYPE( type ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		p_act->setType( static_cast<OPI::Enum::ActionType>(NUM2UINT( type )) );
	}
	OPI_CATCH( "OPIRuby::actionSetType" );
	return Qnil;
}

static VALUE
actionState( VALUE self, VALUE id )
{
	VALUE ret = 0;
	try
	{
		if ( TYPE( id ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->state( NUM2UINT( id ) ) );
	}
	OPI_CATCH( "OPIRuby::actionState" );
	return ret;
}

static VALUE
actionStop( VALUE self )
{
	try
	{
		Action * p_act = instance<Action>( self );
		p_act->stop();
	}
	OPI_CATCH( "OPIRuby::actionStop" );
	return Qnil;
}

static VALUE
actionType( VALUE self )
{
	VALUE ret = 0;
	try
	{
		Action * p_act = instance<Action>( self );
		ret = UINT2NUM( p_act->type() );
	}
	OPI_CATCH( "OPIRuby::actionType" );
	return ret;
}

static VALUE
actionDefine( VALUE self, VALUE howto )
{
	try
	{
		Action * p_act = instance<Action>( self );
		NComposite nmc;
		assignMDC( reinterpret_cast<MDComposite &>(nmc), howto );
		p_act->define( nmc );
	}
	OPI_CATCH( "OPIRuby::actionDefine" );
	return Qnil;
}

static VALUE
actionHowto( VALUE self, VALUE r_ary )
{
	try
	{
		Action * p_act = instance<Action>( self );
		assignRbArray( r_ary, reinterpret_cast<const MDComposite &>(p_act->HOWTO()) );
	}
	OPI_CATCH( "OPIRuby::actionHowto" );
	return r_ary;
}

static VALUE
actionSpec( VALUE self, VALUE r_ary )
{
	try
	{
		Action * p_act = instance<Action>( self );
		assignRbArray( r_ary, reinterpret_cast<const MDComposite &>(p_act->spec()) );
	}
	OPI_CATCH( "OPIRuby::actionSpec" );
	return r_ary;
}

static VALUE
actionView( VALUE self, VALUE report )
{
	try
	{
		Action * p_act = instance<Action>( self );
		MDComposite mdc;
		p_act->view( mdc );
		assignRbArray( report, mdc );
	}
	OPI_CATCH( "OPIRuby::actionView" );
	return Qnil;
}

// OPI::PEntity::ActionSingle
////////////////////////////////////////////////////////////////////////////////////////////////

static  VALUE
actionSingleNew( VALUE self,
	VALUE name,
	VALUE meth,
	VALUE type,
	VALUE acti,
	VALUE stack_size )
{
	try
	{
		if ( TYPE( self ) == T_NIL ||
			 name == Qnil || TYPE( name ) != T_STRING ||
			 meth == Qnil ||
			 TYPE( type ) != T_FIXNUM ||
			 TYPE( acti ) != T_FIXNUM ||
			 TYPE( stack_size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		string nm ( STR2CSTR( name ) );
		WrapFunctor * p_wf = new WrapFunctor( meth );
		auto_ptr<ActionFunctor> p_af ( new ActionFunctor( *p_wf, &WrapFunctor::actFunc ) );
		ActionSingle * p_act = new (OPI::withrow) ActionSingle( nm,
				p_af.get(),
				static_cast<OPI::Enum::ActionType>(NUM2UINT( type )),
				static_cast<OPI::Enum::ActionActivate>(NUM2UINT( acti )),
				NUM2UINT( stack_size ) );

		p_wf->setAction( nm );
		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_act) ) );
		rb_iv_set( self,
				"@m_wrap_func",
				UINT2NUM( reinterpret_cast<UInt32>(p_wf) ) );
	}
	OPI_CATCH( "OPIRuby::actionSingleNew" );
	return self;
}

// OPI::Interact::Invoke
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
invokeNew( VALUE self,
	VALUE obj_pentity,
	VALUE obj_action,
	VALUE invoker,
	VALUE in_size,
	VALUE out_size )
{
	try
	{
		if ( TYPE( obj_pentity ) != T_STRING ||
			 TYPE( obj_action ) != T_STRING ||
			 TYPE( invoker ) == T_NIL ||
			 TYPE( in_size ) != T_FIXNUM ||
			 TYPE( out_size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		string name = STR2CSTR( rb_iv_get( invoker, "@name" ) );
		UInt32 etype = NUM2UINT( rb_iv_get( invoker, "@entryType" ) );
		UInt32 hid = NUM2UINT( rb_iv_get( invoker, "@hostId" ) );
		UInt32 entry = NUM2UINT( rb_iv_get( invoker, "@entry" ) );

		string o_pentity( STR2CSTR( obj_pentity ) );
		string o_action( STR2CSTR( obj_action ) );
		auto_ptr<Pentry> p_ent( new (OPI::withrow) Pentry( name ) );

		WrapInvoke * p_invk = opi_impl.ari.invoke( o_pentity,
				o_action,
				*p_ent,
				NUM2UINT( in_size ),
				NUM2UINT( out_size ) );

		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_invk) ) );
	}
	OPI_CATCH( "OPIRuby::invokeNew" );
	return self;
}

static VALUE
invokeRelease( VALUE self )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		p_invoc->release();
	}
	OPI_CATCH( "OPIRuby::invokeRelease" );
	return Qnil;
}

static VALUE
invokeCancel( VALUE self )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		OPARGS( args );
		p_invoc->opRun( OP_INVOC_CANCEL, &args );
	}
	OPI_CATCH( "OPIRuby::invokeCancel" );
	return Qnil;
}

static VALUE
invokeExcept( VALUE self, VALUE r_ex )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		const OPIException & ex = p_invoc->m_p_invoke->except();
		rb_iv_set( r_ex,
				"@what",
				rb_str_new2( ex.what().c_str() ) );

		rb_iv_set( r_ex,
				"@source",
				rb_str_new2( ex.source().c_str() ) );
	}
	OPI_CATCH( "OPIRuby::invokeExcept" );
	return r_ex;
}

static VALUE
invokeExecute( VALUE self,
	VALUE mode,
	VALUE timeout )
{
	try
	{
		if ( ( TYPE( mode ) != T_FIXNUM && TYPE( mode ) != T_BIGNUM ) ||
			 TYPE( timeout ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		OPARGS( args );
		UInt32 u = NUM2UINT( mode );
		args.append( &u, sizeof(u) );
		u = NUM2UINT( timeout );
		args.append( &u, sizeof(u) );
		p_invoc->opRun( OP_INVOC_EXEC, &args );
	}
	OPI_CATCH( "OPIRuby::invokeExecute" );
	return Qnil;
}

static VALUE
invokeFinished( VALUE self )
{
	VALUE ret = Qfalse;
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		OPARGS( args );
		UInt32 fin = p_invoc->opRun( OP_INVOC_FINISH, &args );
		if ( fin )
			ret = Qtrue;
	}
	OPI_CATCH( "OPIRuby::invokeFinished" );
	return ret;
}

static VALUE
invokeInvoker( VALUE self, VALUE res )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		assignRbPentry( res, p_invoc->m_p_invoke->invoker() );
	}
	OPI_CATCH( "OPIRuby::invokeInvoker" );
	return res;
}

static VALUE
invokeIOS( VALUE self )
{
	VALUE ret = 0;
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		ret = UINT2NUM( reinterpret_cast<UInt32>(p_invoc->m_p_wios) );
	}
	OPI_CATCH( "OPIRuby::invokeIOS" );
	return ret;
}

static VALUE
invokeIsAccepted( VALUE self )
{
	VALUE ret = 0;
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		ret = UINT2NUM( p_invoc->m_p_invoke->isAccepted() );
	}
	OPI_CATCH( "OPIRuby::invokeIsAccepted" );
	return ret;
}

static VALUE
invokeObjectAction( VALUE self, VALUE r_str )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		r_str = rb_str_new2( p_invoc->m_p_invoke->objectAction().c_str() );
	}
	OPI_CATCH( "OPIRuby::invokeObjectAction" );
	return r_str;
}

static VALUE
invokeObjectPentity( VALUE self, VALUE r_str )
{
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		r_str = rb_str_new2( p_invoc->m_p_invoke->objectPentity().c_str() );
	}
	OPI_CATCH( "OPIRuby::invokeObjectPentity" );
	return r_str;
}

static VALUE
invokeReturnValue( VALUE self )
{
	VALUE ret = 0;
	try
	{
		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		ret = UINT2NUM( p_invoc->m_p_invoke->returnValue() );
	}
	OPI_CATCH( "OPIRuby::invokeReturnValue" );
	return ret;
}

static VALUE
invokeSetBufferSize( VALUE self,
	VALUE in_size,
	VALUE out_size )
{
	try
	{
		if ( TYPE( in_size ) != T_FIXNUM ||
		 	 TYPE( out_size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		p_invoc->m_p_invoke->setBufferSize( NUM2UINT( in_size ), NUM2UINT( out_size ) );
	}
	OPI_CATCH( "OPIRuby::invokeSetBufferSize" );
	return Qnil;
}

static VALUE
invokeSetInvoker( VALUE self, VALUE invoker )
{
	try
	{
		if ( invoker == T_NIL  )
			throw OPIException( "invalidParameter" );

		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		auto_ptr<Pentry> p_pent( new (OPI::withrow) Pentry( NULL, OPI_DEF_SIZE ) );
		string name = STR2CSTR( rb_iv_get( invoker, "@name" ) );
		UInt32 etype = NUM2UINT( rb_iv_get( invoker, "@entryType" ) );
		UInt32 entry = NUM2UINT( rb_iv_get( invoker, "@entry" ) );
		VALUE r_hid = rb_iv_get( invoker, "@hostId" );
		UInt32 sz = NUM2UINT( rb_funcall( r_hid, rb_intern( "size" ), 0 ) );
		if ( sz <= sizeof(UInt32) )
		{
			p_pent->assign( name,
					NUM2UINT( r_hid ),
					static_cast<OPI::Enum::PentryType>(etype),
					entry );
		}
		else
		if ( sz <= sizeof(Int64_t) )
		{
			Int64_t hid;
			p_pent->assign( name,
					hid,
					static_cast<OPI::Enum::PentryType>(etype),
					entry );
		}
		else
		{
			Int128_t hid;
			p_pent->assign( name,
					hid,
					static_cast<OPI::Enum::PentryType>(etype),
					entry );
		}
		p_invoc->m_p_invoke->setInvoker( *p_pent );
	}
	OPI_CATCH( "OPIRuby::invokeSetInvoker" );
	return Qnil;
}

static VALUE
invokeSetObjectAction( VALUE self, VALUE name )
{
	try
	{
		if ( TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		p_invoc->m_p_invoke->setObjectAction( STR2CSTR( name ) );
	}
	OPI_CATCH( "OPIRuby::invokeSetObjectAction" );
	return Qnil;
}

static VALUE
invokeSetObjectPentity( VALUE self,	VALUE name )
{
	try
	{
		if ( TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		WrapInvoke * p_invoc = instance<WrapInvoke>( self );
		p_invoc->m_p_invoke->setObjectPentity( STR2CSTR( name ) );
	}
	OPI_CATCH( "OPIRuby::invokeSetObjectPentity" );
	return Qnil;
}

// OPIOS
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
opiosNew( VALUE self, VALUE p_ios )
{
	try
	{
		if ( TYPE( p_ios ) == T_NIL )
			throw OPIException( "invalidParameter" );

		rb_iv_set( self,
				"@m_instance",
				p_ios );
	}
	OPI_CATCH( "OPIRuby::opiosNew" );
	return self;
}

static VALUE
opiosAvailRead( VALUE self )
{
	VALUE ret = 0;
	try
	{
		WrapIOS * p_wios = instance<WrapIOS>( self );
		UInt32 l = p_wios->m_p_ios->availRead();
		ret = UINT2NUM( l );
	}
	OPI_CATCH( "OPIRuby::opiosAvailRead" );
	return ret;
}

static VALUE
opiosAvailWrite( VALUE self )
{
	VALUE ret = 0;
	try
	{
		WrapIOS * p_wios = instance<WrapIOS>( self );
		UInt32 l = p_wios->m_p_ios->availWrite();
		ret = UINT2NUM( l );
	}
	OPI_CATCH( "OPIRuby::opiosAvailWrite" );
	return ret;
}

static VALUE
opiosEnds( VALUE self )
{
	try
	{
		WrapIOS * p_wios = instance<WrapIOS>( self );
		OPARGS( args );
		p_wios->opRun( OP_IOS_ENDS, &args );
	}
	OPI_CATCH( "OPIRuby::opiosEnds" );
	return Qnil;
}

static VALUE
opiosFlush( VALUE self )
{
	try
	{
		WrapIOS * p_wios = instance<WrapIOS>( self );
		OPARGS( args );
		p_wios->opRun( OP_IOS_FLUSH, &args );
	}
	OPI_CATCH( "OPIRuby::opiosFlush" );
	return Qnil;
}

static VALUE
opiosIsEnds( VALUE self )
{
	VALUE ret = Qfalse;
	try
	{
		WrapIOS * p_wios = instance<WrapIOS>( self );
		if ( p_wios->m_p_ios->isEnds() )
			ret = Qtrue;
	}
	OPI_CATCH( "OPIRuby::opiosIsEnds" );
	return ret;
}

static VALUE
opiosRead( VALUE self,
	VALUE r_str,
	VALUE size,
	VALUE wait_ms )
{
	try
	{
		if ( TYPE( r_str ) != T_STRING ||
			 TYPE( size ) != T_FIXNUM ||
			 ( TYPE( wait_ms ) != T_FIXNUM && TYPE( wait_ms ) != T_BIGNUM ) )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		UInt32 sz = NUM2UINT( size );
		if ( !sz )
			return r_str;

		auto_ptr<Int8> buf ( new Int8[sz + 1] );
		Int8 * p_buf = buf.get();
		::memset( p_buf, 0, sz + 1 );
		OPARGS( args );
		args.append( &p_buf, sizeof(p_buf) );
		args.append( &sz, sizeof(sz) );
		UInt32 u = NUM2UINT( wait_ms );
		args.append( &u, sizeof(u) );
		p_wios->opRun( OP_IOS_READ, &args );
		r_str = rb_str_new( p_buf, sz );
	}
	OPI_CATCH( "OPIRuby::opiosRead" );
	return r_str;
}

static VALUE
opiosReadArray( VALUE self, VALUE r_ary )
{
	try
	{
		if ( TYPE( r_ary ) != T_ARRAY )
			throw OPIException( "invalidParameter(Array)" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		MDComposite mdc;
		OPARGS( args );
		UInt32 u = reinterpret_cast<UInt32>(&mdc);
		args.append( &u, sizeof(u) );
		p_wios->opRun( OP_IOS_READMDC, &args );
		assignRbArray( r_ary, mdc );
	}
	OPI_CATCH( "OPIRuby::opiosReadArray" );
	return r_ary;
}

static VALUE
opiosReadfOnce( VALUE self, VALUE write_func )
{
	VALUE ret = 0;
	try
	{
		if ( write_func == Qnil )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		auto_ptr<WrapFunctor> p_wf ( new WrapFunctor( write_func ) );
		auto_ptr<IOFunctor> p_iof ( new IOFunctor( *p_wf, &WrapFunctor::ioFunc ) );
		OPARGS( args );
		UInt32 u = reinterpret_cast<UInt32>(p_iof.get());
		args.append( &u, sizeof(u) );
		UInt32 l = p_wios->opRun( OP_IOS_READFONCE, &args );
		ret = UINT2NUM( l );
	}
	OPI_CATCH( "OPIRuby::opiosReadfOnce" );
	return ret;
}

static VALUE
opiosReadOnce( VALUE self,
	VALUE r_str,
	VALUE size )
{
	try
	{
		if ( TYPE( r_str ) != T_STRING ||
			 TYPE( size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		UInt32 sz = NUM2UINT( size );
		if ( !sz )
			return r_str;

		auto_ptr<Int8> buf ( new Int8[sz + 1] );
		Int8 *p_buf = buf.get();
		::memset( p_buf, 0, sz + 1 );
		OPARGS( args );
		args.append( &p_buf, sizeof(p_buf) );
		args.append( &sz, sizeof(sz) );
		sz = p_wios->opRun( OP_IOS_READONCE, &args );
		r_str = rb_str_new( p_buf, sz );
	}
	OPI_CATCH( "OPIRuby::opiosReadOnce" );
	return r_str;
}

static VALUE
opiosSetWaitTimeout( VALUE self,
	VALUE wait_ms )
{
	try
	{
		if ( TYPE( wait_ms ) != T_FIXNUM
			 && TYPE( wait_ms ) != T_BIGNUM )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		p_wios->m_p_ios->setWaitTimeout( NUM2UINT( wait_ms ) );
	}
	OPI_CATCH( "OPIRuby::opiosSetWaitTimeout" );
	return Qnil;
}

static VALUE
opiosWrite( VALUE self,
	VALUE r_str,
	VALUE size,
	VALUE wait_ms )
{
	try
	{
		if ( TYPE( r_str ) != T_STRING ||
			 TYPE( size ) != T_FIXNUM ||
			 ( TYPE( wait_ms ) != T_FIXNUM && TYPE( wait_ms ) != T_BIGNUM ) )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		char * p = STR2CSTR( r_str );
		OPARGS( args );
		args.append( &p, sizeof(p) );
		UInt32 u = NUM2UINT( size );
		args.append( &u, sizeof(u) );
		u = NUM2UINT( wait_ms );
		args.append( &u, sizeof(u) );
		p_wios->opRun( OP_IOS_WRITE, &args );
	}
	OPI_CATCH( "OPIRuby::opiosWrite" );
	return self;
}

static VALUE
opiosWriteArray( VALUE self, VALUE r_ary )
{
	try
	{
		if ( TYPE( r_ary ) != T_ARRAY )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		MDComposite mdc;
		assignMDC( mdc, r_ary );
		OPARGS( args );
		UInt32 u = reinterpret_cast<UInt32>(&mdc);
		args.append( &u, sizeof(u) );
		p_wios->opRun( OP_IOS_WRITEMDC, &args );
	}
	OPI_CATCH( "OPIRuby::opiosWriteArray" );
	return self;
}

static VALUE
opiosWritefOnce( VALUE self, VALUE read_func )
{
	VALUE ret = 0;
	try
	{
		if ( read_func == Qnil )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		auto_ptr<WrapFunctor> p_wf ( new WrapFunctor( read_func ) );
		auto_ptr<IOFunctor> p_iof( new IOFunctor( *p_wf, &WrapFunctor::ioFunc ) );
		OPARGS( args );
		UInt32 u = reinterpret_cast<UInt32>(p_iof.get());
		args.append( &u, sizeof(u) );
		UInt32 l = p_wios->opRun( OP_IOS_WRITEFONCE, &args );
		ret = UINT2NUM( l );
	}
	OPI_CATCH( "OPIRuby::opiosWritefOnce" );
	return ret;
}

static VALUE
opiosWriteOnce( VALUE self,
	VALUE r_str,
	VALUE size )
{
	VALUE ret = 0;
	try
	{
		if ( TYPE( r_str ) != T_STRING ||
			 TYPE( size ) != T_FIXNUM )
			throw OPIException( "invalidParameter" );

		WrapIOS * p_wios = instance<WrapIOS>( self );
		char * p = STR2CSTR( r_str );
		OPARGS( args );
		args.append( &p, sizeof(p) );
		UInt32 u = NUM2UINT( size );
		args.append( &u, sizeof(u) );

		u = p_wios->opRun( OP_IOS_WRITEONCE, &args );
		ret = UINT2NUM( u );
	}
	OPI_CATCH( "OPIRuby::opiosWrite" );
	return ret;
}

static VALUE
bstreamReadObject( VALUE self, VALUE keep )
{
	try
	{
		if ( TYPE ( keep ) != T_STRING )
			throw OPIException( "invalidParameter" );

		BStream * p_bs = instance<BStream>( self );
		string dest;
		MDElement getmde;
		*p_bs >> getmde;
		getmde.copy( dest );
		VALUE get = rb_str_new( dest.c_str(), dest.size() );
		keep = rb_funcall( keep,
				rb_intern( "replace" ),
				1,
				get );

		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_bs) ) );
	}
	OPI_CATCH( "OPIRuby::bstreamReadObj")
	return keep;
}

static VALUE
bstreamWriteObject( VALUE self, VALUE source )
{
	try
	{
		if ( TYPE( source ) != T_STRING )
			throw OPIException( "invalidParameter" );

		BStream * p_bs = instance<BStream>( self );
		string src( STR2CSTR( source ) );
		MDElement in( src );
		*p_bs << in;
		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_bs) ) );
	}
	OPI_CATCH( "OPIRuby::bstreamWriteObject")
	return Qnil;
}

// OPI::PEntity::Pentity
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
pentityNew( VALUE self, VALUE name )
{
	try
	{
		if ( TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		Pentity * p_pentity = new (OPI::withrow) Pentity( string( STR2CSTR( name ) ) );
		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_pentity) ) );
	}
	OPI_CATCH( "OPIRuby::pentityNew" );
	return self;
}

static VALUE
pentityClose( VALUE self )
{
	try
	{
		Pentity * p_pentity = instance<Pentity>( self );
		p_pentity->close();
	}
	OPI_CATCH( "OPIRuby::pentityClose" );
	return Qnil;
}

static VALUE
pentityGetPentry( VALUE self, VALUE res )
{
	try
	{
		Pentity * p_pentity = instance<Pentity>( self );
		assignRbPentry( res, p_pentity->getPentry() );
	}
	OPI_CATCH( "OPIRuby::pentityGetpentry" );
	return res;
}

static VALUE
pentityName( VALUE self )
{
	VALUE ret = Qnil;
	try
	{
		Pentity * p_pentity = instance<Pentity>( self );
		ret = rb_str_new2( p_pentity->name().c_str() );
	}
	OPI_CATCH( "OPIRuby::pentityName" );
	return ret;
}

static VALUE
pentityOpen( VALUE self )
{
	try
	{
		Pentity * p_pentity = instance<Pentity>( self );
		p_pentity->open();
	}
	OPI_CATCH( "OPIRuby::pentityOpen" );
	return Qnil;
}

static VALUE
pentityQuery( VALUE self,
	VALUE res,
	VALUE args )
{
	try
	{
		if ( TYPE( res ) != T_ARRAY )
			throw OPIException( "invalidParameter(Array)" );

		Pentity * p_pentity = instance<Pentity>( self );
		MDComposite qry;
		if ( args == Qnil )
			qry.assign( null_mdc );
		else
			assignMDC( qry, args );

		p_pentity->query( qry );
		assignRbArray( res, qry );
	}
	OPI_CATCH( "OPIRuby::pentityQuery" );
	return res;
}

// OPI::PEntity::PentityMain
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
pmainNew( VALUE self, VALUE name )
{
	try
	{
		if ( TYPE( name ) != T_STRING )
			throw OPIException( "invalidParameter" );

		PentityMain * p_pmain = new (OPI::withrow) PentityMain( string( STR2CSTR( name ) ) );
		OPIPMRQ * p_rq = new OPIPMRQ();
		rb_iv_set( self,
				"@m_instance",
				UINT2NUM( reinterpret_cast<UInt32>(p_pmain) ) );
		rb_iv_set( self,
				"@m_rq",
				UINT2NUM( reinterpret_cast<UInt32>(p_rq) ) );
	}
	OPI_CATCH( "OPIRuby::pentityMainNew" );
	return self;
}

static VALUE
pmainAdd( VALUE self, VALUE r_act )
{
	VALUE ret = Qfalse;
	try
	{
		if ( r_act == Qnil )
			throw OPIException( "invalidParameter" );

		PentityMain * p_pmain = instance<PentityMain>( self );
		VALUE r_v = rb_funcall( r_act, rb_intern( "instance" ), 0 );
		Action * p_act = reinterpret_cast<Action *>(NUM2UINT( r_v ));
		p_pmain->add( *p_act );
		r_v = rb_funcall( r_act,
				rb_intern( "wrapFunc" ),
				0 );

		WrapFunctor * p_wf = reinterpret_cast<WrapFunctor *>(NUM2UINT( r_v ));
		r_v = rb_iv_get( self, "@m_rq" );
		p_wf->setPMRQ( reinterpret_cast<OPIPMRQ *>(NUM2UINT( r_v )) );
		ret = Qtrue;
	}
	OPI_CATCH( "OPIRuby::pentityMainAdd" );
	return ret;
}

static VALUE
pmainAddSingle( VALUE self, VALUE r_act )
{
	VALUE ret = Qfalse;
	try
	{
		if ( r_act == Qnil )
			throw OPIException( "invalidParameter" );

		PentityMain * p_pmain = instance<PentityMain>( self );
		VALUE r_v = rb_funcall( r_act, rb_intern( "instance" ), 0 );
		ActionSingle * p_act = reinterpret_cast<ActionSingle *>(NUM2UINT( r_v ));
		p_pmain->addSingle( *p_act );
		r_v = rb_funcall( r_act,
				rb_intern( "wrapFunc" ),
				0 );

		WrapFunctor * p_wf = reinterpret_cast<WrapFunctor *>(NUM2UINT( r_v ));
		r_v = rb_iv_get( self, "@m_rq" );
		p_wf->setPMRQ( reinterpret_cast<OPIPMRQ *>(NUM2UINT( r_v )) );
		ret = Qtrue;
	}
	OPI_CATCH( "OPIRuby::pentityMainAddsingle" );
	return ret;
}

static VALUE
pmainBatchDelegate( VALUE self, VALUE to_pentity )
{
	try
	{
		if ( TYPE( to_pentity ) != T_STRING )
			throw OPIException( "invalidParameter" );

		PentityMain * p_pmain = instance<PentityMain>( self );
		string s( STR2CSTR( to_pentity ) );
		p_pmain->batchDelegate( s );
	}
	OPI_CATCH( "OPIRuby::pentityMainBatchDelegate" );
	return Qnil;
}

static VALUE
pmainBatchDelegateRelease( VALUE self, VALUE to_pentity )
{
	try
	{
		if ( TYPE( to_pentity ) != T_STRING )
			throw OPIException( "invalidParameter" );

		PentityMain * p_pmain = instance<PentityMain>( self );
		p_pmain->batchDelegateRelease( string( STR2CSTR( to_pentity ) ) );
	}
	OPI_CATCH( "OPIRuby::pentityMainBatchDelegateRelease" );
	return Qnil;
}

static VALUE
pmainClose( VALUE self, VALUE stop_all )
{
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		bool b_stop = true;
		if ( stop_all == Qfalse )
			b_stop = false;

		p_pmain->close( b_stop );
	}
	OPI_CATCH( "OPIRuby::pentityMainClosee" );
	return Qnil;
}

static VALUE
pmainCount( VALUE self )
{
	VALUE ret = 0;
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		ret = UINT2NUM( p_pmain->count() );
	}
	OPI_CATCH( "OPIRuby::pentityMainCount" );
	return ret;
}

static VALUE
pmainName( VALUE self )
{
	VALUE ret = Qnil;
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		ret = rb_str_new2( p_pmain->name().c_str() );
	}
	OPI_CATCH( "OPIRuby::pentityMainName" );
	return ret;
}

static VALUE
pmainOpen( VALUE self )
{
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		p_pmain->open();
	}
	OPI_CATCH( "OPIRuby::pentityMainOpen" );
	return Qnil;
}

static VALUE
pmainQuery( VALUE self,
	VALUE res,
	VALUE args )
{
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		MDComposite qry;
		if ( args == Qnil )
			qry.assign( null_mdc );
		else
			assignMDC( qry, args );

		p_pmain->query( qry );
		assignRbArray( res, qry );
	}
	OPI_CATCH( "OPIRuby::pentityMainQuery" );
	return res;
}

static VALUE
pmainRemove( VALUE self, VALUE name )
{
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		p_pmain->remove( string( STR2CSTR( name ) ) );
	}
	OPI_CATCH( "OPIRuby::pentityMainRemove" );
	return Qnil;
}

static VALUE
pmainView( VALUE self, VALUE report )
{
	try
	{
		PentityMain * p_pmain = instance<PentityMain>( self );
		MDComposite rpt( NULL, 32 * OPI::def_size );
		p_pmain->view( rpt );
		assignRbArray( report, rpt );
	}
	OPI_CATCH( "OPIRuby::pentityMainView" );
	return Qnil;
}

static VALUE
pmrqGet( VALUE self )
{
	VALUE req = Qnil;
	VALUE r_ios = Qnil;
	VALUE r_pent = Qnil;
	try
	{
		OPIPMRQ * p_rq = reinterpret_cast<OPIPMRQ *>(NUM2UINT( rb_iv_get( self, "@m_rq" ) ));
		p_rq->wait();
		OPIPMRQ::OPIREQ * p_req = p_rq->get();
		if ( p_req != NULL )
		{
			VALUE rc_ios = rb_const_get( rb_cObject, rb_intern( "OPIOS" ) );
			r_ios = rb_funcall( rc_ios,
					rb_intern( "new" ),
					1,
					UINT2NUM( reinterpret_cast<UInt32>(p_req->m_p_ios) ) );
			RBGCREG( r_ios );
			VALUE rc_pentry = rb_const_get( rb_cObject, rb_intern( "Pentry" ) );
			r_pent = rb_funcall( rc_pentry,
					rb_intern( "new" ),
					4,
					rb_str_new2( "" ),
					0,
					0,
					0 );
			RBGCREG( r_pent );
			assignRbPentry( r_pent, *p_req->m_p_invoker );
			VALUE rc_req = rb_const_get( rb_cObject, rb_intern( "ModAction" ) );
			rc_req = rb_const_get( rc_req, rb_intern( "OPIREQ" ) );
			req = rb_funcall( rc_req,
					rb_intern( "new" ),
					5,
					rb_str_new( p_req->m_name.c_str(), p_req->m_name.size() ),
					r_ios,
					r_pent,
					UINT2NUM( p_req->m_mq ),
					UINT2NUM( reinterpret_cast<UInt32>(&p_req->m_except) ) );
		}
	}
	OPI_CATCH( "OPIPentityMainImpl::getRequest" );
	RBGCUREG( r_ios );
	RBGCUREG( r_pent );
	return req;
}

// gc finalizers
////////////////////////////////////////////////////////////////////////////////////////////////

static VALUE
freeAction( VALUE self, VALUE obj )
{
	try
	{
		VALUE to_free = rb_iv_get( obj, "@m_instance" );
		delete reinterpret_cast<Action *>(NUM2UINT( to_free ));
		to_free = rb_iv_get( obj, "@m_wrap_func" );
		delete reinterpret_cast<WrapFunctor *>(NUM2UINT( to_free ));
	}
	OPI_CATCH( "OPIRuby::freeAction" );
	return Qnil;
}

static VALUE
freeIOS( VALUE self, VALUE wrap )
{
	try
	{
		VALUE to_free = rb_iv_get( wrap, "@m_instance" );
		delete reinterpret_cast<WrapIOS *>(NUM2UINT( to_free ) );
	}
	OPI_CATCH( "OPIRuby::freeIOS" );
	return Qnil;
}

static VALUE
freeInvoke( VALUE self, VALUE wrap )
{
	try
	{
		VALUE to_free = rb_iv_get( wrap, "@m_instance" );
		delete reinterpret_cast<WrapInvoke *>(NUM2UINT( to_free ) );
	}
	OPI_CATCH( "OPIRuby::freeInvoke" );
	return Qnil;
}

static VALUE
freePentity( VALUE self, VALUE wrap )
{
	try
	{
		VALUE to_free = rb_iv_get( wrap, "@m_instance" );
		delete reinterpret_cast<Pentity *>(NUM2UINT( to_free ) );
	}
	OPI_CATCH( "OPIRuby::freePentity" );
	return Qnil;
}

static VALUE
freePMain( VALUE self, VALUE wrap )
{
	try
	{
		VALUE to_free = rb_iv_get( wrap, "@m_instance" );
		delete reinterpret_cast<PentityMain *>(NUM2UINT( to_free ) );
	}
	OPI_CATCH( "OPIRuby::freePentityMain" );
	return Qnil;
}

static VALUE
parzPURI( VALUE self,
	VALUE r_uri,
	VALUE r_peer_id,
	VALUE r_pair_id,
	VALUE r_scheme )
{
	VALUE ret = 0;
	try
	{
		if ( TYPE( r_uri ) != T_STRING )
			throw OPIException( "invalidParameter" );

		string uri( STR2CSTR( r_uri ) );
		string peer;
		string pair;
		string scheme;
		ret = INT2NUM( OPI::URI::parsePairURI( uri, peer, pair, scheme ) );
		r_peer_id = rb_str_new2( peer.c_str() );
		r_pair_id = rb_str_new2( pair.c_str() );
		r_scheme = rb_str_new2( scheme.c_str() );
	}
	OPI_CATCH( "OPIRuby::parsePairURI" );
	return ret;
}

static VALUE
raiseOPIException( VALUE self,
	VALUE what,
	VALUE source )
{
	throw OPIException( STR2CSTR( what ), STR2CSTR( source ) );
}

// initial section
////////////////////////////////////////////////////////////////////////////////////////////////

void Init_OPI( void )
{
	// exception
	rb_define_method( rb_cObject,
			"raiseOPIException",
			RBFUNC( raiseOPIException ),
			2);

	// utility functions
	rb_define_method( rb_cObject,
			"mdAssignRbArray",
			RBFUNC( mdAssignRbArray ),
			1 );
	rb_define_method( rb_cObject,
			"mdCopyRbArray",
			RBFUNC( mdCopyRbArray ),
			1 );
	rb_define_method( rb_cObject,
			"parsePairURI",
			RBFUNC( parzPURI ),
			4 );

	// wrap OPI::PEnpity::Action
	opi_impl.action = rb_define_class( "OPIActionImpl", rb_cObject );
	rb_define_attr( opi_impl.action, "m_instance", 1, 0 );
	rb_define_attr( opi_impl.action, "m_wrap_func", 1, 0 );
	rb_define_method( opi_impl.action,
			"initialize",
			RBFUNC( actionNew ),
			6 );
	rb_define_method( opi_impl.action,
			"activate",
			RBFUNC( actionActivate ),
			1 );
	rb_define_method( opi_impl.action,
			"bind",
			RBFUNC( actionBind ),
			2 );
	rb_define_method( opi_impl.action,
			"delegate",
			RBFUNC( actionDelegate ),
			1 );
	rb_define_method( opi_impl.action,
			"delegateRelease",
			RBFUNC( actionDelegateRelease ),
			0 );
	rb_define_method( opi_impl.action,
			"activateMode",
			RBFUNC( actionActivateMode ),
			0 );
	rb_define_method( opi_impl.action,
			"except",
			RBFUNC( actionExcept ),
			1 );
	rb_define_method( opi_impl.action,
			"finish",
			RBFUNC( actionFinish ),
			1 );
	rb_define_method( opi_impl.action,
			"instanceCount",
			RBFUNC( actionInstanceCount ),
			0 );
	rb_define_method( opi_impl.action,
			"instanceLimit",
			RBFUNC( actionInstanceLimit ),
			0 );
	rb_define_method( opi_impl.action,
			"method",
			RBFUNC( actionGetMethod ),
			0 );
	rb_define_method( opi_impl.action,
			"monitor",
			RBFUNC( actionMonitor ),
			0 );
	rb_define_method( opi_impl.action,
			"name",
			RBFUNC( actionName ),
			0 );
	rb_define_method( opi_impl.action,
			"pentityLink",
			RBFUNC( actionPentityLink ),
			0 );
	rb_define_method( opi_impl.action,
			"requestCount",
			RBFUNC( actionRequestCount ),
			0 );
	rb_define_method( opi_impl.action,
			"setActivateMode",
			RBFUNC( actionSetActivateMode ),
			1 );
	rb_define_method( opi_impl.action,
			"setExcept",
			RBFUNC( actionSetExcept ),
			2 );
	rb_define_method( opi_impl.action,
			"setInstanceLimit",
			RBFUNC( actionSetInstanceLimit ),
			1 );
	rb_define_method( opi_impl.action,
			"setPentityLink",
			RBFUNC( actionSetPentityLink ),
			1 );
	rb_define_method( opi_impl.action,
			"setName",
			RBFUNC( actionSetName ),
			1 );
	rb_define_method( opi_impl.action,
			"setStackSize",
			RBFUNC( actionSetStackSize ),
			1 );
	rb_define_method( opi_impl.action,
			"setType",
			RBFUNC( actionSetType ),
			1 );
	rb_define_method( opi_impl.action,
			"state",
			RBFUNC( actionState ),
			1 );
	rb_define_method( opi_impl.action,
			"stop",
			RBFUNC( actionStop ),
			0 );
	rb_define_method( opi_impl.action,
			"type",
			RBFUNC( actionType ),
			0 );
	rb_define_method( opi_impl.action,
			"define",
			RBFUNC( actionDefine ),
			1 );
	rb_define_method( opi_impl.action,
			"HOWTO",
			RBFUNC( actionHowto ),
			1 );
	rb_define_method( opi_impl.action,
			"spec",
			RBFUNC( actionSpec ),
			0 );
	rb_define_method( opi_impl.action,
			"view",
			RBFUNC( actionView ),
			1 );

	// wrap OPI::PEntity::ActionSingle
	opi_impl.action_single = rb_define_class( "OPIActionSingleImpl", rb_cObject );
	rb_define_attr( opi_impl.action_single, "m_instance", 1, 0 );
	rb_define_attr( opi_impl.action_single, "m_wrap_func", 1, 0 );
	rb_define_method( opi_impl.action_single,
			"initialize",
			RBFUNC( actionSingleNew ),
			5 );
	rb_define_method( opi_impl.action_single,
			"activate",
			RBFUNC( actionActivate ),
			1 );
	rb_define_method( opi_impl.action_single,
			"bind",
			RBFUNC( actionBind ),
			2 );
	rb_define_method( opi_impl.action_single,
			"delegate",
			RBFUNC( actionDelegate ),
			1 );
	rb_define_method( opi_impl.action_single,
			"delegateRelease",
			RBFUNC( actionDelegateRelease ),
			0 );
	rb_define_method( opi_impl.action_single,
			"activateMode",
			RBFUNC( actionActivateMode ),
			0 );
	rb_define_method( opi_impl.action_single,
			"except",
			RBFUNC( actionExcept ),
			0 );
	rb_define_method( opi_impl.action_single,
			"finish",
			RBFUNC( actionFinish ),
			1 );
	rb_define_method( opi_impl.action_single,
			"instanceCount",
			RBFUNC( actionInstanceCount ),
			0 );
	rb_define_method( opi_impl.action_single,
			"instanceLimit",
			RBFUNC( actionInstanceLimit ),
			0 );
	rb_define_method( opi_impl.action_single,
			"method",
			RBFUNC( actionGetMethod ),
			0 );
	rb_define_method( opi_impl.action_single,
			"monitor",
			RBFUNC( actionMonitor ),
			0 );
	rb_define_method( opi_impl.action_single,
			"name",
			RBFUNC( actionName ),
			0 );
	rb_define_method( opi_impl.action_single,
			"pentityLink",
			RBFUNC( actionPentityLink ),
			0 );
	rb_define_method( opi_impl.action_single,
			"requestCount",
			RBFUNC( actionRequestCount ),
			0 );
	rb_define_method( opi_impl.action_single,
			"setActivateMode",
			RBFUNC( actionSetActivateMode ),
			1 );
	rb_define_method( opi_impl.action_single,
			"setExcept",
			RBFUNC( actionSetExcept ),
			2 );
	rb_define_method( opi_impl.action_single,
			"setPentityLink",
			RBFUNC( actionSetPentityLink ),
			1 );
	rb_define_method( opi_impl.action_single,
			"setName",
			RBFUNC( actionSetName ),
			1 );
	rb_define_method( opi_impl.action_single,
			"setStackSize",
			RBFUNC( actionSetStackSize ),
			1 );
	rb_define_method( opi_impl.action_single,
			"setType",
			RBFUNC( actionSetType ),
			1 );
	rb_define_method( opi_impl.action_single,
			"state",
			RBFUNC( actionState ),
			1 );
	rb_define_method( opi_impl.action_single,
			"stop",
			RBFUNC( actionStop ),
			0 );
	rb_define_method( opi_impl.action_single,
			"type",
			RBFUNC( actionType ),
			0 );
	rb_define_method( opi_impl.action_single,
			"define",
			RBFUNC( actionDefine ),
			1 );
	rb_define_method( opi_impl.action_single,
			"HOWTO",
			RBFUNC( actionHowto ),
			1 );
	rb_define_method( opi_impl.action_single,
			"spec",
			RBFUNC( actionSpec ),
			0 );
	rb_define_method( opi_impl.action_single,
			"view",
			RBFUNC( actionView ),
			1 );

	// wrap OPI::Interact::Invoke
	opi_impl.invoke = rb_define_class( "OPInvokeImpl", rb_cObject );
	rb_define_method( opi_impl.invoke,
			"initialize",
			RBFUNC( invokeNew ),
			5 );
	rb_define_method( opi_impl.invoke,
			"release",
			RBFUNC( invokeRelease ),
			0 );
	rb_define_method( opi_impl.invoke,
			"cancel",
			RBFUNC( invokeCancel ),
			0 );
	rb_define_method( opi_impl.invoke,
			"except",
			RBFUNC( invokeExcept ),
			1 );
	rb_define_method( opi_impl.invoke,
			"execute",
			RBFUNC( invokeExecute ),
			2 );
	rb_define_method( opi_impl.invoke,
			"finished",
			RBFUNC( invokeFinished ),
			0 );
	rb_define_method( opi_impl.invoke,
			"invoker",
			RBFUNC( invokeInvoker ),
			1 );
	rb_define_method( opi_impl.invoke,
			"IOS",
			RBFUNC( invokeIOS ),
			0 );
	rb_define_method( opi_impl.invoke,
			"isAccepted",
			RBFUNC( invokeIsAccepted ),
			0 );
	rb_define_method( opi_impl.invoke,
			"objectAction",
			RBFUNC( invokeObjectAction ),
			1 );
	rb_define_method( opi_impl.invoke,
			"objectPentity",
			RBFUNC( invokeObjectPentity ),
			1 );
	rb_define_method( opi_impl.invoke,
			"returnValue",
			RBFUNC( invokeReturnValue ),
			0 );
	rb_define_method( opi_impl.invoke,
			"setBufferSize",
			RBFUNC( invokeSetBufferSize ),
			2 );
	rb_define_method( opi_impl.invoke,
			"setInvoker",
			RBFUNC( invokeSetInvoker ),
			1 );
	rb_define_method( opi_impl.invoke,
			"setObjectAction",
			RBFUNC( invokeSetObjectAction ),
			1 );
	rb_define_method( opi_impl.invoke,
			"setObjectPentity",
			RBFUNC( invokeSetObjectPentity ),
			1 );

	// wrap OPIOS
	opi_impl.ios = rb_define_class( "OPIOSImpl", rb_cObject );
	rb_define_method( opi_impl.ios,
			"initialize",
			RBFUNC( opiosNew ),
			1 );
	rb_define_method( opi_impl.ios,
			"availRead",
			RBFUNC( opiosAvailRead ),
			0 );
	rb_define_method( opi_impl.ios,
			"availWrite",
			RBFUNC( opiosAvailWrite ),
			0 );
	rb_define_method( opi_impl.ios,
			"ends",
			RBFUNC( opiosEnds ),
			0 );
	rb_define_method( opi_impl.ios,
			"flush",
			RBFUNC( opiosFlush ),
			0 );
	rb_define_method( opi_impl.ios,
			"isEnds",
			RBFUNC( opiosIsEnds ),
			0 );
	rb_define_method( opi_impl.ios,
			"read",
			RBFUNC( opiosRead ),
			3 );
	rb_define_method( opi_impl.ios,
			"readArray",
			RBFUNC( opiosReadArray ),
			1 );
	rb_define_method( opi_impl.ios,
			"readfOnce",
			RBFUNC( opiosReadfOnce ),
			1 );
	rb_define_method( opi_impl.ios,
			"readOnce",
			RBFUNC( opiosReadOnce ),
			2 );
	rb_define_method( opi_impl.ios,
			"setWaitTimeout",
			RBFUNC( opiosSetWaitTimeout ),
			1 );
	rb_define_method( opi_impl.ios,
			"write",
			RBFUNC( opiosWrite ),
			3 );
	rb_define_method( opi_impl.ios,
			"writeArray",
			RBFUNC( opiosWriteArray ),
			1 );
	rb_define_method( opi_impl.ios,
			"writefOnce",
			RBFUNC( opiosWritefOnce ),
			1 );
	rb_define_method( opi_impl.ios,
			"writeOnce",
			RBFUNC( opiosWriteOnce ),
			2 );
	rb_define_method( opi_impl.ios,
			"readObject",
			RBFUNC( bstreamReadObject ),
			1 );
	rb_define_method( opi_impl.ios,
			"writeObject",
			RBFUNC( bstreamWriteObject ),
			1 );

	// wrap OPI::PEntity::Pentity
	opi_impl.pentity = rb_define_class( "OPIPentityImpl", rb_cObject );
	rb_define_method( opi_impl.pentity,
			"initialize",
			RBFUNC( pentityNew ),
			1 );
	rb_define_method( opi_impl.pentity,
			"close",
			RBFUNC( pentityClose ),
			0 );
	rb_define_method( opi_impl.pentity,
			"getPentry",
			RBFUNC( pentityGetPentry ),
			1 );
	rb_define_method( opi_impl.pentity,
			"name",
			RBFUNC( pentityName ),
			0 );
	rb_define_method( opi_impl.pentity,
			"open",
			RBFUNC( pentityOpen ),
			0 );
	rb_define_method( opi_impl.pentity,
			"query",
			RBFUNC( pentityQuery ),
			2 );

	// wrap OPI::PEntity::PentityMain
	opi_impl.pmain = rb_define_class( "OPIPentityMainImpl", rb_cObject );
	rb_define_method( opi_impl.pmain,
			"initialize",
			RBFUNC( pmainNew ),
			1 );
	rb_define_method( opi_impl.pmain,
			"addAction",
			RBFUNC( pmainAdd ),
			1 );
	rb_define_method( opi_impl.pmain,
			"addActionSingle",
			RBFUNC( pmainAddSingle ),
			1 );
	rb_define_method( opi_impl.pmain,
			"batchDelegate",
			RBFUNC( pmainBatchDelegate ),
			1 );
	rb_define_method( opi_impl.pmain,
			"batchDelegateRelease",
			RBFUNC( pmainBatchDelegateRelease ),
			1 );
	rb_define_method( opi_impl.pmain,
			"close",
			RBFUNC( pmainClose ),
			1 );
	rb_define_method( opi_impl.pmain,
			"count",
			RBFUNC( pmainCount ),
			0 );
	rb_define_method( opi_impl.pmain,
			"getRequest",
			RBFUNC( pmrqGet ),
			0 );
	rb_define_method( opi_impl.pmain,
			"name",
			RBFUNC( pmainName ),
			0 );
	rb_define_method( opi_impl.pmain,
			"open",
			RBFUNC( pmainOpen ),
			0 );
	rb_define_method( opi_impl.pmain,
			"query",
			RBFUNC( pmainQuery ),
			2 );
	rb_define_method( opi_impl.pmain,
			"remove",
			RBFUNC( pmainRemove ),
			1 );
	rb_define_method( opi_impl.pmain,
			"view",
			RBFUNC( pmainView ),
			1 );

	// gc finializers
	rb_define_method( rb_cObject,
			"freeAction",
			RBFUNC( freeAction ), 1 );
	rb_define_method( rb_cObject,
			"freeIOS",
			RBFUNC( freeIOS ),
			1 );
	rb_define_method( rb_cObject,
			"freeInvoke",
			RBFUNC( freeInvoke ),
			1 );
	rb_define_method( rb_cObject,
			"freePentity",
			RBFUNC( freePentity ),
			1 );
	rb_define_method( rb_cObject,
			"freePentityMain",
			RBFUNC( freePMain ),
			1 );

}

void main()
{}
