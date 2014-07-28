///////////////////////////////////////////////////////////////////////////////////////////////////
//
// clist.h
// Copyright (c) 2009 by Xie Yun.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef H_CLIST
#define H_CLIST

#include "stree.h"

namespace Common
{
// CList
//   chunk list, doubly-linked
///////////////////////////////////////////////////////////////////////////////////////////////////

template< class _field, unsigned int _columns >
struct CNode
{
    _field m_field[_columns];
};

template< class _field,
    unsigned int _col,
    class _alloc = default_alloc >
class CList 
    : public STree<CNode<_field, _col>, 1, _alloc>
{
public:
    typedef CList<_field, _col, _alloc> Self;
    typedef STree<CNode<_field, _col>, 1, _alloc> Super;
    typedef _field Field;
    typedef _alloc Allocator;
    enum
    {
        columns = _col
    };

protected:
    typedef typename Super::_Node _Node;
    typedef typename _Node::Nodeptr Nodeptr;
    typedef typename _Node::PNode PNode;
    typedef Field * PField;
    
    class _FIterator
    {
        typedef _FIterator Self;
    public:
        _FIterator( PField p = 0 ) throw()
            {
                this->reset( p );
            }

        _FIterator( const Self & rhs ) throw()
            {
                this->reset( rhs.get() );
            }

        Field & operator* () const throw()
            {
                return *(this->get());
            }

        PField operator-> () const throw()
            {
                return this->get();
            }

        PField get() const throw()
            {
                return ( this->m_ptr + this->m_fi );
            }

        void reset( PField p ) throw()
            {
                this->m_ptr = p;
                this->m_fi = 0;
            }

        Self & operator= ( PField rhs ) throw()
            {
                this->reset( rhs );
                return *this;
            }

        Self & operator= ( const Self & rhs ) throw()
            {
                this->reset( rhs.get() );
                return *this;
            }

        bool operator== ( PField rhs ) const throw()
            {
                return ( this->get() == rhs );
            }

        bool operator== ( const Self & rhs ) const throw()
            {
                return ( this->get() == rhs.get() );
            }

        bool operator!= ( PField rhs ) const throw()
            {
                return ( !this->operator==( rhs ) );
            }

        bool operator!= ( const Self & rhs ) const throw()
            {
                return this->operator!=( rhs.get() );
            }

        Self & next() throw()
            {
                this->m_fi++;
                if ( this->m_fi >= columns )
                {
                    this->m_fi = 0;
                    this->m_ptr = TYPE<PField>(TYPE<PNode>(this->m_ptr)->next());
                }
                return *this;
            }

    private:
        UInt32 m_fi;
        PField m_ptr;
    };

public:
    typedef typename PNode pointer;
    typedef typename Nodeptr node_pointer;
    typedef typename PField field_pointer;
    typedef typename Super::pointer super_pointer;
    typedef _FIterator field_iterator;

    CList( UInt32 count = 0, const Allocator & alloc = Allocator() ) throw()
        : Super( count, alloc )
        {}

    CList( const Self & rhs ) throw()
        : Super( rhs )
        {}

    ~CList() throw()
        {}
};

} // namespace Common
#endif // H_CLIST
