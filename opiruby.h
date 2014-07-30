////////////////////////////////////////////////////////////////////////////////////////////////
//
// opiruby.h
// Copyright (c) 2006 by Xie Yun. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////

#include "opi.h"
#include "ruby.h"

#ifdef close
#undef close
#endif

#ifdef bind
#undef bind
#endif

#ifdef read
#undef read
#endif

#ifdef write
#undef write
#endif

#define OPI_CATCH( method )\
	catch ( OPIException & ex )\
	{\
		ex.appendSource( method );\
		string excpt( ex.what() );\
		excpt.append( 1, '\n' ).append( ex.source() );\
		VALUE rc_ex = rb_const_get( rb_cObject, rb_intern( "OPIException" ) );\
		rb_raise( rc_ex, excpt.c_str() );\
	}\
	catch ( ... )\
	{\
		string excpt( "unknownException\n" );\
		excpt.append( method );\
		VALUE rc_ex = rb_const_get( rb_cObject, rb_intern("OPIException") );\
		rb_raise( rc_ex, excpt.c_str() );\
	}

#define RBFUNC( f )		(unsigned long(__cdecl*)( ... )) f
#define RBGCREG( a )	rb_gc_register_address( &a )
#define RBGCUREG( a )	if ( a != Qnil ) rb_gc_unregister_address( &a )

// data types
////////////////////////////////////////////////////////////////////////////////////////////////

#define OPARG_SIZE			256
#define OPARGS( a )			MDComposite a( NULL, OPARG_SIZE )

class ActRbInvoc;
struct OPIPMRQ;
struct WrapFunctor;
struct WrapInvoke;
struct WrapIOS;

enum EnumOperation
{
	OP_NONE = 0,
	OP_FIRST = 1,
	OP_ACT_FINISH = OP_FIRST,
	OP_INVOC_CANCEL,
	OP_INVOC_EXEC,
	OP_INVOC_FINISH,
	OP_IOS_FIRST,
	OP_IOS_ENDS = OP_IOS_FIRST,
	OP_IOS_FLUSH,
	OP_IOS_READ,
	OP_IOS_READMDC,
	OP_IOS_READONCE,
	OP_IOS_READFONCE,
	OP_IOS_WRITE,
	OP_IOS_WRITEMDC,
	OP_IOS_WRITEONCE,
	OP_IOS_WRITEFONCE,
	OP_IOS_LAST = OP_IOS_WRITEFONCE,
	OP_LAST = OP_IOS_LAST
};

// ActRbInvoc
////////////////////////////////////////////////////////////////////////////////////////////////

class ActRbInvoc : public Action
{
public:
	ActRbInvoc()
		: Action( "RbInvoc", NULL, ACTIVE_ACTION, ACTIVATE_IMMEDIATE, 1024 )
		{};

	void handle( ActInstance & t_ins,
			const MDCHead & req,
			void * p = NULL );
	WrapInvoke * invoke( const string & obj_pentity,
			const string & obj_action,
			const Pentry & invoker,
			UInt32 in_size,
			UInt32 out_size );
};

// OPIPMRQ
////////////////////////////////////////////////////////////////////////////////////////////////

struct OPIPMRQ
{
	typedef struct OPIREQ
	{
		string m_name;
		const WrapIOS * m_p_ios;
		const Pentry * m_p_invoker;
		System::MsgQueue m_mq;
		OPIException m_except;

		OPIREQ( const string & name = "",
			const WrapIOS * p_ios = NULL,
			const Pentry * p_invoker = NULL,
			System::MsgQueue mq = 0,
			const OPIException & ex = OPIException() )
			: m_name( name ),
			m_p_ios( p_ios ),
			m_p_invoker( p_invoker ),
			m_mq( mq ),
			m_except( ex )
			{};
	};

	typedef LQueue< deque< OPIREQ * >, OPIREQ * > REQue;
	//typedef deque< OPIREQ * > reque;
	
	OPIPMRQ()
		//: m_crsec( NULL )
		{
			//this->m_crsec = new ::CRITICAL_SECTION;
			//::InitializeCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			this->m_reque.clear();
		};

	~OPIPMRQ()
		{
			this->m_reque.clear();
			//if ( this->m_crsec )
			//{
			//	::DeleteCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			//}
		};

	bool empty() const throw()
		{
			return this->m_reque.empty();
		};

	OPIREQ * get()
		{
			if ( this->m_reque.empty() )
				return NULL;

			return this->m_reque.get();
			//::EnterCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			//OPIREQ * p_item = this->m_reque.front();
			//this->m_reque.pop_front();
			//::LeaveCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			//return p_item;
		};

	void put( OPIREQ & req )
		{
			//::EnterCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			//this->m_reque.push_back( &req );
			//::LeaveCriticalSection( (LPCRITICAL_SECTION)this->m_crsec );
			this->m_reque.put( &req );
			this->m_sig_recv.raise();
		};

	void wait()
		{
			if ( ! this->m_reque.empty() )
				return;

			this->m_sig_recv.wait( 0 );
		};

	//void * m_crsec;
	//reque m_reque;
	REQue m_reque;
	System::Event m_sig_recv;
};

// WrapFunctor
////////////////////////////////////////////////////////////////////////////////////////////////

class WrapFunctor
{
public:
	WrapFunctor( VALUE rb_method )
		: m_method( rb_method ),
		m_action( "" ),
		m_pmrq( NULL )
		{};

	Int32 actFunc( BStream & t_ios, const Pentry & invoker );
	Int32 ioFunc( SBlock & s_b );
	
	VALUE getMethod()
		{
			return this->m_method;
		};

	void setAction( const string & name )
		{
			this->m_action = name;
		};

	void setPMRQ( OPIPMRQ * pmrq )
		{
			this->m_pmrq = pmrq;
		};

private:
	string m_action;
	VALUE m_method;
	OPIPMRQ * m_pmrq;
};

// WrapIOS
////////////////////////////////////////////////////////////////////////////////////////////////

struct WrapIOS
{
	WrapIOS( BStream & t_ios, System::MsgQueue mq )
		: m_mq( mq ), m_p_ios( &t_ios )
		{};
	void opHandler( UInt32 op, void * p );
	Int32 opRun( UInt32 op, void * p_args );

	System::MsgQueue m_mq;
	BStream * m_p_ios;
	System::Event m_sig;
};

// WrapInvoke
////////////////////////////////////////////////////////////////////////////////////////////////

struct WrapInvoke 
{
	typedef struct WIArgs
	{
		string obj_pentity;
		string obj_action;
		string invoker;
		UInt32 in_size;
		UInt32 out_size;
	} WIArgs;

	WrapInvoke( const string & obj_pentity,
		const string & obj_action,
		const Pentry & invoker,
		UInt32 in_size,
		UInt32 out_size )
		: m_mq( 0 ),
		m_p_invoke( NULL ),
		m_p_wios( NULL )
		{
			m_args.obj_pentity = obj_pentity;
			m_args.obj_action = obj_action;
			m_args.invoker = invoker.name();
			m_args.in_size = in_size;
			m_args.out_size = out_size;
		};
	~WrapInvoke()
		{
			OPI::freePtr( this->m_p_invoke );
			OPI::freePtr( this->m_p_wios );
		};
	void opHandler( UInt32 op, void * p );
	Int32 opRun( UInt32 op, void * p_args );
	void release();

	WIArgs m_args;
	System::MsgQueue m_mq;
	Invoke * m_p_invoke;
	WrapIOS * m_p_wios;
	System::Event m_sig;
};

// OPImpl
////////////////////////////////////////////////////////////////////////////////////////////////

struct OPImpl
{
	VALUE	action;
	VALUE	action_single;
	VALUE	invoke;
	VALUE	ios;
	VALUE	pentity;
	VALUE	pmain;
	ActRbInvoc ari;
} opi_impl;

