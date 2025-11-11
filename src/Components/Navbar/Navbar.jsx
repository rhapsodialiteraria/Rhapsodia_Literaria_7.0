import React, { useState, useRef, useEffect } from 'react';
import './Navbar.css';
import logo from '../../assets/logo.svg';
import menu_open from '../../assets/menu_open.svg';
import menu_close from '../../assets/menu_close.svg';
import underline from '../../assets/nav_underline.svg';
import AnchorLink from 'react-anchor-link-smooth-scroll';

const Navbar = () => {
    const [menu, setMenu] = useState("home");
    const [isMenuOpen, setIsMenuOpen] = useState(false);
    const [isProgramsPopupOpen, setIsProgramsPopupOpen] = useState(false);
    const menuRef = useRef(null);

    const toggleMenu = () => {
        if (menuRef.current) {
            menuRef.current.style.right = isMenuOpen ? "-350px" : "0";
            setIsMenuOpen(!isMenuOpen);
        }
    };

    const handleMenuClick = (menuName) => {
        setMenu(menuName);
        if (window.innerWidth <= 768) toggleMenu();
    };

    const toggleProgramsPopup = () => {
        setIsProgramsPopupOpen(!isProgramsPopupOpen);
    };

    const handleProgramClick = (sectionId) => {
        setIsProgramsPopupOpen(false);
        handleMenuClick("programs");
        const element = document.querySelector(sectionId);
        if (element) {
            element.scrollIntoView({ behavior: "smooth" });
        }
    };

    // ✅ Close menu when clicking outside (for mobile)
    useEffect(() => {
        const handleClickOutside = (event) => {
            if (
                menuRef.current &&
                !menuRef.current.contains(event.target) &&
                !event.target.classList.contains("nav-mob-open")
            ) {
                menuRef.current.style.right = "-350px";
                setIsMenuOpen(false);
            }
        };
        if (isMenuOpen) document.addEventListener("click", handleClickOutside);
        else document.removeEventListener("click", handleClickOutside);
        return () => document.removeEventListener("click", handleClickOutside);
    }, [isMenuOpen]);

    return (
        <div className='navbar'>
            <img src={logo} alt="Logo" className='nav-logo' />

            {/* Mobile Menu Buttons */}
            {!isMenuOpen && (
                <img
                    src={menu_open}
                    alt="Open Menu"
                    className='nav-mob-open'
                    onClick={toggleMenu}
                />
            )}
            {isMenuOpen && (
                <img
                    src={menu_close}
                    alt="Close Menu"
                    className='nav-mob-close'
                    onClick={toggleMenu}
                />
            )}

            <ul className='nav-menu' ref={menuRef}>
                <li onClick={() => handleMenuClick("home")}>
                    <AnchorLink className="anchor-link" offset={50} href="#home">Home</AnchorLink>
                    {menu === "home" && <img src={underline} alt="underline" />}
                </li>

                {/* Programs dropdown */}
                <li className="programs-menu" onClick={toggleProgramsPopup}>
                    <p className="anchor-link">Events ▾</p>
                    {menu === "programs"}
                    {isProgramsPopupOpen && (
                        <div className="programs-popup">
                            <p onClick={() => handleProgramClick("#day1")}>Day 1</p>
                            <p onClick={() => handleProgramClick("#day2")}>Day 2</p>
                        </div>
                    )}
                </li>



                <li onClick={() => handleMenuClick("contact")}>
                    <AnchorLink className="anchor-link" offset={50} href="#contact">Contact Us</AnchorLink>
                    {menu === "contact" && <img src={underline} alt="underline" />}
                </li>
                {/* ✅ Sponsors section link */}
                <li onClick={() => handleMenuClick("sponsors")}>
                    <AnchorLink className="anchor-link" offset={50} href="#sponsors">Sponsors</AnchorLink>
                    {menu === "sponsors" && <img src={underline} alt="underline" />}
                </li>
            </ul>

            <div className='nav-connect'>
                <AnchorLink className="anchor-link" href="#contact">Connect Us</AnchorLink>
            </div>
        </div>
    );
};

export default Navbar;
