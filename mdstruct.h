///////////////////////////////////////////////////////////////////////////////////////////////////
//
// mdstruct.h
// Copyright (c) 2008 by Xie Yun. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _H_MDSTRUCT
#define _H_MDSTRUCT

#include "mdata.h"

//#ifndef TYPE
//#include "typedefs.h"
//#endif

#ifndef PINT8
#define PINT8( x ) \
    TYPE<UInt8 *>( x )

#define VAR_PMDC( x, y ) \
    VAR( MDCHead *, x, y )
#endif

// MDStruct
///////////////////////////////////////////////////////////////////////////////////////////////////

class MDStruct
    : private MDComposite
{
    friend struct _BField;
private:
    struct _At
    {
        MDStruct * m_this;
        UInt32 m_pos;
    };

    struct _BField
    {
        _At m_at;
        _BField()
            {
                this->m_at.m_this = TYPE<_At *>(PINT8(this) - sizeof(_BField))->m_this;;
                this->m_at.m_pos = this->m_at.m_this->m_at.m_pos++;
            };
    };

private:
    _At m_at;

    void init() throw()
        {
            this->m_at.m_pos = 0;
            this->m_at.m_this = this;
        };

public:
    MDStruct()
        {
            this->init();
        };

    MDStruct( const MDStruct & rhs )
    	: MDComposite( rhs )
    	{
    		this->init();
    	};

    explicit MDStruct( const MDCHead & rhs )
        : MDComposite( rhs )
        {
            this->init();
        };

    explicit MDStruct( const SBlock & block )
        : MDComposite( block.p, block.size )
        {
            this->init();
        };

    using MDComposite::block;
    using MDComposite::blockPtr;
    using MDComposite::blockSize;
    using MDComposite::count;
    using MDComposite::append;
    using MDComposite::operator [];

    bool operator== ( const MDStruct & rhs ) const
        {
            return ( ::memcmp( &this->block(), &rhs.block(), this->block().size ) == 0 );
        };

    template< typename _vt > void insert( UInt32 pos, const _vt & value )
        {
            if ( pos < this->count() )
                return;

            this->append( value.block() );
        };

    void insert( UInt32 pos, UInt32 value )
        {
            if ( pos < this->count() )
                return;

            this->append( &value, sizeof(value) );
        };

    void insert( UInt32 pos, void * value )
        {
            if ( pos < this->count() )
                return;

            this->append( &value, sizeof(value) );
        };

    void insert( UInt32 pos, const string & value )
        {
            if ( pos < this->count() )
                return;

            this->append( value.c_str(), value.size() );
        };

    template< typename _vt > void update( UInt32 pos, const _vt & value )
        {
            this->setItem( pos, value.block() );
        };

    void update( UInt32 pos, UInt32 value )
        {
            this->setItem( pos, &value, sizeof(value) );
        };

    void update( UInt32 pos, void * value )
        {
            this->setItem( pos, &value, sizeof(value) );
        };

    void update( UInt32 pos, const string & value )
        {
            this->setItem( pos, value );
        };

    UInt32 & fieldCount() throw()
        {
            return this->m_at.m_pos;
        };

public:
    template< typename _field >
    class MDField
        : private _BField
    {
    public:
        typedef _field Field;
        MDField( const Field & v = Field() )
            {
                this->m_at.m_this->insert( this->m_at.m_pos, v );
            };

        Field operator() () const throw()
            {
                VAR_PMDC( p_item, (*this->m_at.m_this)[this->m_at.m_pos] );
                if ( ! p_item  )
                    return Field();

                return Field( *p_item );
            };

        void operator= ( const Field & v )
            {
                this->m_at.m_this->update( this->m_at.m_pos, v );
            };
    };

    template<>
    class MDField<string>
        : private _BField
    {
    public:
        typedef string Field;
        //MDField<string>( const Field & v = 0 ) // VC++6
        MDField( const Field & v = Field( "string" ) )
            {
                this->m_at.m_this->insert( this->m_at.m_pos, v );
            };

        Field operator() () const throw()
            {
                VAR_PMDC( p_item, (*this->m_at.m_this)[this->m_at.m_pos] );
                if ( ! p_item  )
                    return Field();

                return Field( TYPE<Field::pointer>( mdeData( *p_item ) ), p_item->size );
            };

        void operator= ( const Field & value )
            {
                this->m_at.m_this->update( this->m_at.m_pos, value );
            };

        void operator= ( const char * value )
            {
                this->m_at.m_this->update( this->m_at.m_pos, string( value ) );
            };
    };

    template<>
    class MDField<UInt32>
        : private _BField
    {
    public:
        typedef UInt32 Field;
        MDField( const Field & v = Field() )
            {
                this->m_at.m_this->insert( this->m_at.m_pos, v );
            };

        Field operator() () const throw()
            {
                VAR_PMDC( p_item, (*this->m_at.m_this)[this->m_at.m_pos] );
                if ( ! p_item  )
                    return Field();

                return *TYPE<Field *>( mdeData( *p_item ) );
            };

        void operator= ( const Field & v )
            {
                this->m_at.m_this->update( this->m_at.m_pos, v );
            };
    };

    template< typename _pointer >
    class MDPointer
        : private _BField
    {
    public:
        typedef _pointer Pointer;
        MDPointer( const Pointer v = 0 )
            {
                this->m_at.m_this->insert( this->m_at.m_pos, TYPE<void *>( v ) );
            };

        Pointer operator() () const throw()
            {
                VAR_PMDC( p_item, (*this->m_at.m_this)[this->m_at.m_pos] );
                if ( ! p_item  )
                    return 0;

                return *TYPE<Pointer *>( mdeData( *p_item ) );
            };

        void operator= ( const Pointer v )
            {
                this->m_at.m_this->update( this->m_at.m_pos, TYPE<void *>( v ) );
            };
    };

    template< typename _field >
    class _MDArray
        :private _BField
    {
    public:
        typedef _field Field;
        using _BField::m_at;
        _MDArray( )
            {
                MDComposite mdc;
                this->m_at.m_this->insert( this->m_at.m_pos, mdc );                 
            };
            
        Field operator[]( UInt32 i ) const throw()
            {
                MDCHead * p_head = (*this->m_at.m_this)[ this->m_at.m_pos ]; 
                MDComposite mdc( p_head, p_head->size  );
                if ( i >= mdc.count() )
                    return Field();
                return Field( *mdc[ i ] );
            };
           
        void append( const Field & v )
            {
                MDCHead * p_head = ( *this->m_at.m_this )[ this->m_at.m_pos ]; 
                MDComposite mdc( *p_head );
                mdc.append( v );
                this->m_at.m_this->update( this->m_at.m_pos, mdc );
            };
        
        void update( UInt32 pos, const Field & v )
            {
                MDCHead * p_head = ( *this->m_at.m_this )[ this->m_at.m_pos ]; 
                MDComposite mdc( *p_head );
                mdc.setItem( pos, v );
                this->m_at.m_this->update( this->m_at.m_pos, mdc );
            };
        
        UInt32 count()
            {
                MDCHead * p_head = (*this->m_at.m_this)[ this->m_at.m_pos ]; 
                MDComposite mdc( p_head, p_head->size  );
                return mdc.count();                
            };
         
        void remove( UInt32 pos )
            {
                MDCHead * p_head = ( *this->m_at.m_this )[ this->m_at.m_pos ]; 
                MDComposite mdc( *p_head );
                mdc.erase( pos );
                this->m_at.m_this->update( this->m_at.m_pos, mdc );
            };    
    };    
    
    template< typename _field >
    class MDArray 
        : public _MDArray<_field> 
    {
    };
        
    template<>
    class MDArray<string> 
        : public _MDArray<string>
    {
    public:
        Field operator[]( UInt32 i ) const throw()
            {
                MDCHead * p_head = (*this->m_at.m_this)[ this->m_at.m_pos ]; 
                MDComposite mdc( p_head, p_head->size  );
                if ( i >= mdc.count() )
                    return Field();
                p_head = mdc[i];
                return Field( TYPE<Field::pointer>( mdeData( *p_head ) ), p_head->size );
            };
    };

    template<>
    class MDArray<UInt32> 
        : public _MDArray<UInt32>
    {
        typedef UInt32 Field;
    public:
        Field operator[]( UInt32 i ) const throw()
            {
                MDCHead * p_head = (*this->m_at.m_this)[ this->m_at.m_pos ]; 
                MDComposite mdc( p_head, p_head->size  );
                if ( i >= mdc.count() )
                    return Field();
                p_head = mdc[i];
                Field v = *TYPE<Field *>( mdeData( *p_head ) );
                return *TYPE<Field *>( mdeData( *p_head ) );
            };

        void append( const Field & v )
            {
                MDCHead * p_head = ( *this->m_at.m_this )[ this->m_at.m_pos ]; 
                MDComposite mdc( *p_head );
                mdc.append( &v, sizeof( v ) );
                this->m_at.m_this->update( this->m_at.m_pos, mdc );
            };
        
        void update( UInt32 pos, const Field & v )
            {
                MDCHead * p_head = ( *this->m_at.m_this )[ this->m_at.m_pos ]; 
                MDComposite mdc( *p_head );
                mdc.setItem( pos, &v, sizeof( v ) );
                this->m_at.m_this->update( this->m_at.m_pos, mdc );
            };
    };

};

#define MD_CONSTRUCTOR( class_name ) \
	class_name( const class_name & rhs ) : MDStruct( rhs ) {}; \
    explicit class_name( const MDCHead & rhs ) : MDStruct( rhs ) {}; \
    explicit class_name( const SBlock & rhs ) : MDStruct( rhs ) {};

#endif // _H_MDSTRUCT
